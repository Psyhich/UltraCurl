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
	private:
		// Because of language limitations I cannot put this in the end of class
		// For better clarity using simple struct instead of std::tuple
		struct WorkingTuple
		{
			CURI m_URIToDownload;
			FDownloadCallback m_fCallback;
			Downloaders::CHTTPDownloader m_downloader{nullptr};

			WorkingTuple(const CURI &newURI) : 
				m_URIToDownload{newURI}
			{

			}

		};
	public:
		CConcurrentDownloader(unsigned uMaxCountOfThreads) noexcept : 
			m_runningDownloadsLock{new(std::nothrow) std::mutex()},
			m_runningDownloads{new(std::nothrow) std::list<std::pair<std::thread, WorkingTuple>>()},
			m_uriQueueLock{new(std::nothrow) std::mutex()},
			m_urisToDownload(new(std::nothrow) std::queue<WorkingTuple>()),
			m_uMaxCountOfThreads{uMaxCountOfThreads},
			m_bShouldStop{new(std::nothrow) bool(false)},
			m_bCanAddNewTasks{new(std::nothrow) bool(true)}
		{

		}
		CConcurrentDownloader() noexcept :
			m_runningDownloadsLock{new(std::nothrow) std::mutex()},
			m_runningDownloads{new(std::nothrow) std::list<std::pair<std::thread, WorkingTuple>>()},
			m_uriQueueLock{new(std::nothrow) std::mutex()},
			m_urisToDownload(new(std::nothrow) std::queue<WorkingTuple>()),
			m_uMaxCountOfThreads{std::thread::hardware_concurrency()},
			m_bShouldStop{new(std::nothrow) bool(false)},
			m_bCanAddNewTasks{new(std::nothrow) bool(true)}
		{

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
			if(*m_bShouldStop || !*m_bCanAddNewTasks)
			{
				return;
			}
			std::unique_lock<std::mutex> urisLock{*m_uriQueueLock};

			WorkingTuple workerData{cURIToDownload};
			workerData.m_fCallback = fCallback;

			m_urisToDownload->push(std::move(workerData));
			urisLock.unlock();

			LoadQueueIntoDownloads();
		}

		void JoinTasksCompletion() noexcept
		{
			*m_bCanAddNewTasks = false;

			// Waiting till queue is empty
			while(!m_urisToDownload->empty())
			{

			}

			for(auto &pair : *m_runningDownloads)
			{
				std::scoped_lock<std::mutex> downloadsLock{*m_runningDownloadsLock};
				if(pair.first.joinable())
				{
					pair.first.join();
				}
			}
		}

		bool IsDone() const noexcept
		{
			return m_runningDownloads->empty() && m_urisToDownload->empty();
		}

		std::multimap<CURI, std::tuple<std::size_t, std::size_t>> GetDownloadProgres()
		{
			std::scoped_lock<std::mutex> downloadLock{ *m_runningDownloadsLock };
			std::multimap<CURI, std::tuple<std::size_t, std::size_t>> progressData;
			for(auto &[thread, worker] : *m_runningDownloads)
			{
				if(auto bytesProgress = worker.m_downloader.GetProgress())
				{
					progressData.insert({worker.m_URIToDownload, *bytesProgress});
				}
			}

			return progressData;
		}
		~CConcurrentDownloader() noexcept
		{
			// Setting stop flag if we can do that
			if(m_bShouldStop && m_bCanAddNewTasks)
			{
				*m_bShouldStop = true;
				*m_bCanAddNewTasks = false;
			}

			// Clearing queue of URIs put for download
			if(m_uriQueueLock)
			{
				std::scoped_lock<std::mutex> queueLock{*m_uriQueueLock};
				std::queue<WorkingTuple>().swap(*m_urisToDownload);
			}

			// Joining all threads if we can
			if(m_runningDownloadsLock)
			{
				m_runningDownloadsLock->lock();
				for(auto &dataPair : *m_runningDownloads)
				{
					if(dataPair.first.joinable())
					{
						dataPair.first.join();
					}
				}
				m_runningDownloadsLock->unlock();
			}

			DealocateResources();
		}
	private:
		void MoveData(CConcurrentDownloader &&downloaderToMove) noexcept
		{
				//JoinTasksCompletion();

				std::scoped_lock<std::mutex> downloadsLock {*downloaderToMove.m_runningDownloadsLock};
				std::scoped_lock<std::mutex> queueLock {*downloaderToMove.m_uriQueueLock};

				DealocateResources();

				m_runningDownloads = downloaderToMove.m_runningDownloads;
				m_urisToDownload = downloaderToMove.m_urisToDownload;

				m_runningDownloadsLock = downloaderToMove.m_runningDownloadsLock;
				m_uriQueueLock = downloaderToMove.m_uriQueueLock;

				m_bShouldStop = downloaderToMove.m_bShouldStop;
				m_bCanAddNewTasks = downloaderToMove.m_bCanAddNewTasks;

				downloaderToMove.m_runningDownloadsLock = nullptr;
				downloaderToMove.m_uriQueueLock = nullptr;
				
				downloaderToMove.m_bShouldStop = nullptr;
				downloaderToMove.m_bCanAddNewTasks = nullptr;

				downloaderToMove.m_runningDownloads = nullptr;
				downloaderToMove.m_urisToDownload = nullptr;
		}

		inline void DealocateResources() noexcept
		{
			delete m_runningDownloadsLock;
			delete m_uriQueueLock;
			delete m_bShouldStop;
			delete m_bCanAddNewTasks;
			
			delete m_urisToDownload;
			delete m_runningDownloads;
		}

		void DownloadTask(WorkingTuple &pWorkingData) noexcept
		{
			std::optional<HTTP::CHTTPResponse> gotData;
			do
			{
				pWorkingData.m_downloader = 
					Downloaders::CHTTPDownloader(std::unique_ptr<SocketClass>(new SocketClass()));
				gotData = 
					pWorkingData.m_downloader.Download(pWorkingData.m_URIToDownload);
			}
			while(pWorkingData.m_fCallback(std::move(gotData)) && !m_bShouldStop);

			// If we already stoping execution, we don't need to remove task here
			if(!*m_bShouldStop)
			{
				DeleteTaskByID(std::this_thread::get_id());

				// Loading new values into queue
				LoadQueueIntoDownloads();
			}
		}

		void DeleteTaskByID(std::thread::id nThreadID) noexcept
		{
			std::scoped_lock<std::mutex> downloadsLock{*m_runningDownloadsLock};
			auto foundID = std::find_if(m_runningDownloads->begin(), 
				m_runningDownloads->end(), 
				[nThreadID](const auto &pair) -> bool {
					return pair.first.get_id() == nThreadID;
				});

			// Detaching current thread because it will be deleted in next line
			foundID->first.detach();
			m_runningDownloads->erase(foundID);
		}

		void LoadQueueIntoDownloads() noexcept
		{
			std::scoped_lock<std::mutex> downloadsLock(*m_runningDownloadsLock);
			std::scoped_lock<std::mutex> queueLock(*m_uriQueueLock);

			while(m_runningDownloads->size() < m_uMaxCountOfThreads && !m_urisToDownload->empty())
			{
				m_runningDownloads->push_back(
					std::make_pair(std::thread(), std::move(m_urisToDownload->front())));
				m_urisToDownload->pop();

				auto &downloadPair = m_runningDownloads->back();

				downloadPair.first = 
					std::thread(&Concurrency::CConcurrentDownloader<SocketClass>::DownloadTask,		
						this, std::ref(downloadPair.second));
			}
		}
	private:
		std::mutex *m_runningDownloadsLock{nullptr};
		std::list<std::pair<std::thread, WorkingTuple>> *m_runningDownloads;

		std::mutex *m_uriQueueLock{nullptr};
		std::queue<WorkingTuple> *m_urisToDownload;

		unsigned m_uMaxCountOfThreads;
		bool *m_bShouldStop{nullptr};
		bool *m_bCanAddNewTasks{nullptr};
	};
}

#endif // DOWNLOADER_POOL_H
