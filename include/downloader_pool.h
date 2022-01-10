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
	template<class SocketClass>
	class CConcurrentDownloader
	{
	static_assert(std::is_base_of<Sockets::CSocket, SocketClass>(), "SocketClass should inherit CSocket");
	private:
		// Because of language limitations I cannot put this in the end of class
		// For better clarity using simple struct instead of std::tuple
		struct WorkingTuple
		{
			Downloaders::CHTTPDownloader<SocketClass> m_downloader;
			CURI m_URIToDownload;
			std::thread m_runningThread;
			FDownloadCallback m_fCallback;

			WorkingTuple(const CURI &newURI) : m_URIToDownload{newURI}
			{

			}
		};
	public:
		CConcurrentDownloader(unsigned uMaxCountOfThreads) noexcept : 
			m_runningDownloadsLock{new(std::nothrow) std::mutex()},
			m_uriQueueLock{new(std::nothrow) std::mutex()},
			m_uMaxCountOfThreads{uMaxCountOfThreads},
			m_bShouldStop{new(std::nothrow) bool(false)},
			m_bCanAddNewTasks{new(std::nothrow) bool(true)}
		{

		}
		CConcurrentDownloader() noexcept :
			m_runningDownloadsLock{new(std::nothrow) std::mutex()},
			m_uriQueueLock{new(std::nothrow) std::mutex()},
			m_uMaxCountOfThreads{std::thread::hardware_concurrency()},
			m_bShouldStop{new(std::nothrow) bool(false)},
			m_bCanAddNewTasks{new(std::nothrow) bool(true)}
		{

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
				std::queue<std::pair<std::size_t, WorkingTuple>>().swap(m_urisToDownload);
			}

			// Joining all threads if we can
			if(m_runningDownloadsLock)
			{
				m_runningDownloadsLock->lock();
				for(auto &dataPair : m_runningDownloads)
				{
					if(dataPair.second.m_runningThread.joinable())
					{
						dataPair.second.m_runningThread.join();
					}
				}
				m_runningDownloadsLock->unlock();
			}

			DealocateResources();
		}
		
		// We cannot copy threads
		CConcurrentDownloader(const CConcurrentDownloader &cCopyOfDownloader) = delete;
		CConcurrentDownloader &operator =(const CConcurrentDownloader &cCopyOfDownloader) = delete;
		// We can change their owner
		CConcurrentDownloader(CConcurrentDownloader &&downloaderToMove) noexcept
		{
			MoveData(downloaderToMove);
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

			m_urisToDownload.push(std::make_pair(m_nCurrentID, std::move(workerData)));
			++m_nCurrentID;
			urisLock.unlock();

			LoadQueueIntoDownloads();
		}

		void JoinTasksCompletion() noexcept
		{
			*m_bCanAddNewTasks = false;

			std::scoped_lock<std::mutex> queueLock(*m_uriQueueLock);
			for(auto &[uID, worker] : m_runningDownloads)
			{
				std::scoped_lock<std::mutex> downloadsLock{*m_runningDownloadsLock};
				if(worker.m_runningThread.joinable())
				{
					worker.m_runningThread.join();
				}
			}
		}
	private:
		void MoveData(CConcurrentDownloader &&downloaderToMove) noexcept
		{
				m_runningDownloads.swap(downloaderToMove.m_runningDownloads);
				m_urisToDownload.swap(downloaderToMove.m_urisToDownload);

				DealocateResources();

				m_runningDownloadsLock = downloaderToMove.m_runningDownloadsLock;
				m_uriQueueLock = downloaderToMove.m_uriQueueLock;
				
				m_bShouldStop = downloaderToMove.m_bShouldStop;
				m_bCanAddNewTasks = downloaderToMove.m_bCanAddNewTasks;

				downloaderToMove.m_runningDownloadsLock = nullptr;
				downloaderToMove.m_uriQueueLock = nullptr;
				
				downloaderToMove.m_bShouldStop = nullptr;
				downloaderToMove.m_bCanAddNewTasks = nullptr;
		}

		inline void DealocateResources() noexcept
		{
			delete m_runningDownloadsLock;
			delete m_uriQueueLock;
			delete m_bShouldStop;
			delete m_bCanAddNewTasks;
		}

		void DownloadTask(std::pair<std::size_t, WorkingTuple> &dataPair) noexcept
		{
			std::optional<HTTP::CHTTPResponse> gotData;
			do
			{
				gotData = dataPair.second.m_downloader.Download(dataPair.second.m_URIToDownload);
			}
			while(dataPair.second.m_fCallback(std::move(gotData)) && !m_bShouldStop);

			// If we already stoping execution, we don't need to remove task here
			if(!*m_bShouldStop)
			{
				DeleteTaskByID(dataPair.first);

				// Loading new values into map
				LoadQueueIntoDownloads();
			}
		}

		void DeleteTaskByID(std::size_t nIndex) noexcept
		{
			if(!*m_bCanAddNewTasks)
			{
				return;
			}

			std::scoped_lock<std::mutex> downloadsLock{*m_runningDownloadsLock};
			for(auto iter = m_runningDownloads.begin(); iter != m_runningDownloads.end(); iter++)
			{
				if(iter->first == nIndex)
				{
					m_runningDownloads.erase(iter);
				}
			}
		}

		void LoadQueueIntoDownloads() noexcept
		{
			if(!*m_bCanAddNewTasks)
			{
				return;
			}

			std::scoped_lock<std::mutex> queueLock(*m_uriQueueLock);
			std::scoped_lock<std::mutex> downloadsLock(*m_runningDownloadsLock);

			while(m_runningDownloads.size() < m_uMaxCountOfThreads && !m_urisToDownload.empty())
			{
				m_runningDownloads.push_back(std::move(m_urisToDownload.front()));
				m_urisToDownload.pop();
				std::pair<std::size_t, WorkingTuple> &queuePair = std::ref(m_runningDownloads.back());

				queuePair.second.m_runningThread = 
					std::thread(&Concurrency::CConcurrentDownloader<SocketClass>::DownloadTask, this, std::ref(queuePair));

			}
		}
	private:
		std::size_t m_nCurrentID{0};

		std::mutex *m_runningDownloadsLock{nullptr};
		std::list<std::pair<std::size_t, WorkingTuple>> m_runningDownloads;

		std::mutex *m_uriQueueLock{nullptr};
		std::queue<std::pair<std::size_t, WorkingTuple>> m_urisToDownload;

		unsigned m_uMaxCountOfThreads;
		bool *m_bShouldStop{nullptr};
		bool *m_bCanAddNewTasks{nullptr};
	};
}

#endif // DOWNLOADER_POOL_H
