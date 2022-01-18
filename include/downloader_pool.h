#ifndef DOWNLOADER_POOL_H
#define DOWNLOADER_POOL_H

#include <optional>
#include <thread>
#include <functional>
#include <map>
#include <queue>
#include <mutex>
#include <vector>
#include <list>

#include "sockets.h"
#include "http_downloader.h"
#include "response.h"
#include "uri.h"

namespace Downloaders::Concurrency
{
	/// Callback that should accept HTTP response and should specify if download should be repeated
	using FDownloadCallback = 
		std::function<bool(std::optional<HTTP::CHTTPResponse>&&)>;

	/// Special thread pool that carries conccurent HTTP download
	/// provided by SocketClass that should inherit CSocket class
	/// After each download running callback function with passed response
	template<class SocketClass> class CConcurrentDownloader
	{
	static_assert(std::is_base_of<Sockets::CSocket, SocketClass>(), "SocketClass should inherit CSocket");
	using TaskInProgress = std::tuple<std::thread, Downloaders::CHTTPDownloader, CURI>;
	using TaskNonExecuted = std::pair<CURI, FDownloadCallback>;

	public:
		CConcurrentDownloader(unsigned uMaxCountOfThreads) noexcept : 
			m_pRunningDownloadsLock{new std::mutex()},
			m_pRunningDownloads{new std::list<TaskInProgress>()},
			m_pUriQueueLock{new std::mutex()},
			m_pUrisToDownload(new std::queue<TaskNonExecuted>()),
			m_uMaxCountOfThreads{uMaxCountOfThreads},
			m_pbShouldStop{new bool(false)},
			m_pbCanAddNewTasks{new bool(true)}
		{

		}
		CConcurrentDownloader() noexcept :
			m_pRunningDownloadsLock{new std::mutex()},
			m_pRunningDownloads{new std::list<TaskInProgress>()},
			m_pUriQueueLock{new std::mutex()},
			m_pUrisToDownload(new std::queue<TaskNonExecuted>()),
			m_pbShouldStop{new bool(false)},
			m_pbCanAddNewTasks{new bool(true)}
		{
			m_uMaxCountOfThreads = std::thread::hardware_concurrency();
			// If hardware_concurrency() failed to determine count of hardware supported threads assuming constant number of them
			if(m_uMaxCountOfThreads == 0)
			{
				m_uMaxCountOfThreads = DEFAULT_COUNT_OF_THREADS;
			}
		}
		
		// We cannot copy threads
		CConcurrentDownloader(const CConcurrentDownloader &cCopyOfDownloader) = delete;
		CConcurrentDownloader &operator =(const CConcurrentDownloader &cCopyOfDownloader) = delete;

		// We can change their owner
		CConcurrentDownloader(CConcurrentDownloader &&downloaderToMove) noexcept
		{
			MoveData(std::move(downloaderToMove));
		}
		CConcurrentDownloader &operator =(CConcurrentDownloader &&downloaderToMove) noexcept
		{
			if(this != &downloaderToMove)
			{
				MoveData(std::move(downloaderToMove));
			}

			return *this;
		}

		/// Function to add new URI for download
		void AddNewTask(const CURI &cURIToDownload, FDownloadCallback fCallback) noexcept
		{
			// We can't add new tasks if we already stoped processing
			if(*m_pbShouldStop || !m_pbCanAddNewTaskss)
			{
				return;
			}
			std::unique_lock<std::mutex> urisLock{*m_pUriQueueLock};

			m_pUrisToDownload->push({cURIToDownload, fCallback});
			urisLock.unlock();

			LoadQueueIntoDownloads();
		}

		void JoinTasksCompletion() noexcept
		{
			*m_pbCanAddNewTasks = false;

			// Waiting till queue is empty
			while(!m_pUrisToDownload->empty())
			{

			}

			for(auto &pair : *m_pRunningDownloads)
			{
				std::scoped_lock<std::mutex> downloadsLock{*m_pRunningDownloadsLock};
				if(std::thread &runningThread = std::get<0>(pair);
					runningThread.joinable())
				{
					runningThread.join();
				}
			}
		}

		bool IsDone() const noexcept
		{
			return m_pRunningDownloads->empty() &&m_pUrisToDownloadd->empty();
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
			if(m_pbShouldStop &&m_pbCanAddNewTaskss)
			{
				*m_pbShouldStop = true;
				*m_pbCanAddNewTasks = false;
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
		void MoveData(CConcurrentDownloader &&downloaderToMove) noexcept
		{
				std::scoped_lock<std::mutex> downloadsLock {*downloaderToMove.m_pRunningDownloadsLock};
				std::scoped_lock<std::mutex> queueLock {*downloaderToMove.m_pUriQueueLock};

				DealocateResources();

				m_pRunningDownloads = downloaderToMove.m_pRunningDownloads;
				m_pUrisToDownload = downloaderToMove.m_pUrisToDownload;

				m_pRunningDownloadsLock = downloaderToMove.m_pRunningDownloadsLock;
				m_pUriQueueLock = downloaderToMove.m_pUriQueueLock;

				m_pbShouldStop = downloaderToMove.m_pbShouldStop;
				m_pbCanAddNewTasks = downloaderToMove.m_pbCanAddNewTasks;

				downloaderToMove.m_pRunningDownloadsLock = nullptr;
				downloaderToMove.m_pUriQueueLock = nullptr;
				
				downloaderToMove.m_pbShouldStop = nullptr;
				downloaderToMove.m_pbCanAddNewTasks = nullptr;

				downloaderToMove.m_pRunningDownloads = nullptr;
				downloaderToMove.m_pUrisToDownload = nullptr;
		}

		inline void DealocateResources() noexcept
		{
			delete m_pRunningDownloadsLock;
			delete m_pUriQueueLock;
			delete m_pbShouldStop;
			delete m_pbCanAddNewTasks;
			
			delete m_pUrisToDownload;
			delete m_pRunningDownloads;
		}

		void DownloadTask(TaskInProgress &taskInProgress, FDownloadCallback fCallback) noexcept
		{
			std::optional<HTTP::CHTTPResponse> gotData;
			auto &[thread, downloader, uri] = taskInProgress;
			do
			{
				downloader = Downloaders::CHTTPDownloader(std::unique_ptr<SocketClass>(new SocketClass()));
				gotData = downloader.Download(uri);
			}
			while(fCallback(std::move(gotData)) && !m_pbShouldStop);

			// If we already stoping execution, we don't need to remove task here
			if(!*m_pbShouldStop)
			{
				DeleteTaskByID(std::this_thread::get_id());

				// Loading new values into queue
				LoadQueueIntoDownloads();
			}
		}

		void DeleteTaskByID(std::thread::id nThreadID) noexcept
		{
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
			std::scoped_lock<std::mutex> downloadsLock(*m_pRunningDownloadsLock);
			std::scoped_lock<std::mutex> queueLock(*m_pUriQueueLock);

			while(m_pRunningDownloads->size() < m_uMaxCountOfThreads && m_pUrisToDownloadd->empty())
			{
				auto [uri, callback] = std::move(m_pUrisToDownload->front());
				m_pRunningDownloads->push_back(
					std::make_tuple(std::thread(), 
						Downloaders::CHTTPDownloader(std::unique_ptr<SocketClass>(new SocketClass())), uri));
				m_pUrisToDownload->pop();

				auto &downloadPair = m_pRunningDownloads->back();

				std::get<0>(downloadPair) = 
					std::thread(&Concurrency::CConcurrentDownloader<SocketClass>::DownloadTask,
						this, std::ref(downloadPair), callback);
			}
		}
	private:
		static const constexpr unsigned DEFAULT_COUNT_OF_THREADS{2};

		std::mutex *m_pRunningDownloadsLock{nullptr};
		std::list<TaskInProgress> *m_pRunningDownloads;

		std::mutex *m_pUriQueueLock{nullptr};
		std::queue<TaskNonExecuted> *m_pUrisToDownload;

		unsigned m_uMaxCountOfThreads;
		bool *m_pbShouldStop{nullptr};
		bool *m_pbCanAddNewTasks{nullptr};
	};
}

#endif // DOWNLOADER_POOL_H
