#ifndef DOWNLOADER_POOL_H
#define DOWNLOADER_POOL_H

#include <optional>
#include <thread>
#include <functional>
#include <map>
#include <queue>
#include <mutex>
#include <vector>
#include <atomic>
#include <list>

#include "http_downloader.h"
#include "response.h"
#include "uri.h"

namespace Downloaders::Concurrency
{
	/// Callback that should accept HTTP response and should specify if download should be repeated
	using FDownloadCallback = 
		std::function<bool(std::optional<HTTP::CHTTPResponse>&&)>;

	using FSocketFactory = 
		std::function<Downloaders::CHTTPDownloader::SocketClass(const CURI& cURIToProcess)>;

	/// Special thread pool that carries conccurent HTTP download
	/// provided by SocketClass that should inherit CSocket class
	/// After each download running callback function with passed response
	class CConcurrentDownloader
	{
	using TaskInProgress = std::tuple<std::thread, Downloaders::CHTTPDownloader, CURI>;
	using TaskNonExecuted = std::pair<CURI, FDownloadCallback>;

	public:
		// We cannot copy or move threads because we can move pointer to the pool
		CConcurrentDownloader(const CConcurrentDownloader &cCopyOfDownloader) = delete;
		CConcurrentDownloader &operator =(const CConcurrentDownloader &cCopyOfDownloader) = delete;

		CConcurrentDownloader(CConcurrentDownloader &&downloaderToMove) = delete;
		CConcurrentDownloader &operator =(CConcurrentDownloader &&downloaderToMove) = delete;


		// Creational method for heap-only allocation
		static CConcurrentDownloader *AllocatePool(FSocketFactory fFactory, unsigned uCountOfThreads = 0) noexcept
		{
			if(uCountOfThreads == 0)
			{
				return new(std::nothrow) CConcurrentDownloader(fFactory);
			}
			return new(std::nothrow) CConcurrentDownloader(fFactory, uCountOfThreads);
		}

		/// Function to add new URI for download
		void AddNewTask(const CURI &cURIToDownload, FDownloadCallback fCallback) noexcept
		{
			// We can't add new tasks if we already stoped processing
			if(*m_pbShouldStop || !*m_pbCanAddNewTasks)
			{
				return;
			}

			std::unique_lock<std::mutex> urisLock{*m_pUriQueueLock};
				m_pUrisToDownload->push({cURIToDownload, fCallback});
			urisLock.unlock();

			m_pbIsDone->store(false);

			LoadQueueIntoDownloads();
		}

		void JoinTasksCompletion() noexcept
		{
			m_pbCanAddNewTasks->store(false);

			// Waiting till queue is empty
			while(!IsDone())
			{

			}

			std::scoped_lock<std::mutex> downloadsLock{*m_pRunningDownloadsLock};
			for(auto &pair : *m_pRunningDownloads)
			{
				if(std::thread &runningThread = std::get<0>(pair);
					runningThread.joinable())
				{
					runningThread.join();
				}
			}
		}

		bool IsDone() const noexcept
		{
			return m_pbIsDone->load();
		}

		std::multimap<CURI, std::tuple<std::size_t, std::size_t>> GetDownloadProgres()
		{
			std::scoped_lock<std::mutex> downloadLock{ *m_pRunningDownloadsLock };
			std::multimap<CURI, std::tuple<std::size_t, std::size_t>> progressData;
			for(auto &[thread, downloader, uri] : *m_pRunningDownloads)
			{
				if(auto bytesProgress = downloader.GetProgress())
				{
					progressData.insert({uri, *bytesProgress});
				}
			}

			return progressData;
		}
		~CConcurrentDownloader() noexcept
		{
			// Setting stop flag if we can do that
			if(m_pbShouldStop && m_pbCanAddNewTasks)
			{
				m_pbShouldStop->store(true);
				m_pbCanAddNewTasks->store(false);
			}

			// Clearing queue of URIs put for download
			if(m_pUriQueueLock)
			{
				std::scoped_lock<std::mutex> queueLock{*m_pUriQueueLock};
				std::queue<TaskNonExecuted>().swap(*m_pUrisToDownload);
			}

			// Joining all threads if we can
			if(m_pRunningDownloadsLock)
			{
				m_pRunningDownloadsLock->lock();
				for(auto &dataPair : *m_pRunningDownloads)
				{
					if(std::thread &runningThread = std::get<0>(dataPair); 
						runningThread.joinable())
					{
						runningThread.join();
					}
				}
				m_pRunningDownloadsLock->unlock();
			}

			DealocateResources();
		}
	private:
		// Default constructor is hidden to prohibit on stack creation
		CConcurrentDownloader(FSocketFactory fFactory, unsigned uMaxCountOfThreads) noexcept : 
			m_fCurrentSocketFactory{fFactory},
			m_pRunningDownloadsLock{new std::mutex()},
			m_pRunningDownloads{new std::list<TaskInProgress>()},
			m_pUriQueueLock{new std::mutex()},
			m_pUrisToDownload(new std::queue<TaskNonExecuted>()),
			m_uMaxCountOfThreads{uMaxCountOfThreads},
			m_pbShouldStop{new std::atomic<bool>(false)},
			m_pbCanAddNewTasks{new std::atomic<bool>(true)},
			m_pbIsDone{new std::atomic<bool>(false)}
		{

		}
		CConcurrentDownloader(FSocketFactory fFactory) noexcept :
			m_fCurrentSocketFactory{fFactory},
			m_pRunningDownloadsLock{new std::mutex()},
			m_pRunningDownloads{new std::list<TaskInProgress>()},
			m_pUriQueueLock{new std::mutex()},
			m_pUrisToDownload(new std::queue<TaskNonExecuted>()),
			m_pbShouldStop{new std::atomic<bool>(false)},
			m_pbCanAddNewTasks{new std::atomic<bool>(true)},
			m_pbIsDone{new std::atomic<bool>(false)}
		{
			m_uMaxCountOfThreads = std::thread::hardware_concurrency();
			// If hardware_concurrency() failed to determine count of hardware supported threads assuming constant number of them
			if(m_uMaxCountOfThreads == 0)
			{
				m_uMaxCountOfThreads = DEFAULT_COUNT_OF_THREADS;
			}
		}
		
		inline void DealocateResources() noexcept
		{
			delete m_pRunningDownloadsLock;
			delete m_pUriQueueLock;
			
			delete m_pUrisToDownload;
			delete m_pRunningDownloads;

			delete m_pbShouldStop;
			delete m_pbCanAddNewTasks;
			delete m_pbIsDone;
		}

		void DownloadTask(TaskInProgress &taskInProgress, FDownloadCallback fCallback) noexcept
		{
			std::optional<HTTP::CHTTPResponse> gotData;
			auto &[thread, downloader, uri] = taskInProgress;
			do
			{
				gotData = downloader.Download(uri);
			}
			while(fCallback(std::move(gotData)) && !m_pbShouldStop);

			// If we already stoping execution, we don't need to remove task here
			DeleteTaskByID(std::this_thread::get_id());

			// Loading new values into queue
			LoadQueueIntoDownloads();
		}

		void DeleteTaskByID(std::thread::id nThreadID) noexcept
		{
			if(m_pbShouldStop->load())
			{
				return;
			}

			std::scoped_lock<std::mutex> downloadsLock{*m_pRunningDownloadsLock};
			auto foundID = std::find_if(m_pRunningDownloads->begin(), 
				m_pRunningDownloads->end(), 
				[nThreadID](const auto &taksTuple) -> bool {
					return std::get<0>(taksTuple).get_id() == nThreadID;
				});

			// Detaching current thread because it will be deleted in next line
			std::thread &currentThread = std::get<0>(*foundID);
			if(currentThread.joinable())
			{
				currentThread.detach();
			}
			m_pRunningDownloads->erase(foundID);
		}

		void LoadQueueIntoDownloads() noexcept
		{
			if(m_pbShouldStop->load())
			{
				return;
			}

			std::scoped_lock<std::mutex> queueLock(*m_pUriQueueLock);
			std::scoped_lock<std::mutex> downloadsLock(*m_pRunningDownloadsLock);

			while(m_pRunningDownloads->size() < m_uMaxCountOfThreads && !m_pUrisToDownload->empty())
			{
				auto [uri, callback] = std::move(m_pUrisToDownload->front());

				m_pRunningDownloads->push_back(
					std::make_tuple(std::thread(), 
						Downloaders::CHTTPDownloader(m_fCurrentSocketFactory(uri)), uri));
				m_pUrisToDownload->pop();

				auto &downloadPair = m_pRunningDownloads->back();

				std::get<0>(downloadPair) = 
					std::thread(&Concurrency::CConcurrentDownloader::DownloadTask,
						this, std::ref(downloadPair), callback);
			}

			if(m_pUrisToDownload->empty() && m_pRunningDownloads->empty())
			{
				m_pbIsDone->store(true);
			}

		}
	private:
		static const constexpr unsigned DEFAULT_COUNT_OF_THREADS{2};

		const FSocketFactory m_fCurrentSocketFactory;

		std::mutex *m_pRunningDownloadsLock{nullptr};
		std::list<TaskInProgress> *m_pRunningDownloads;

		std::mutex *m_pUriQueueLock{nullptr};
		std::queue<TaskNonExecuted> *m_pUrisToDownload;

		unsigned m_uMaxCountOfThreads;
		std::atomic<bool> *m_pbShouldStop{nullptr};
		std::atomic<bool> *m_pbCanAddNewTasks{nullptr};
		std::atomic<bool> *m_pbIsDone{nullptr};
	};
}

#endif // DOWNLOADER_POOL_H
