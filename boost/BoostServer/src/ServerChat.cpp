#include<iostream>
#include<list>
#include<map>
#include<queue>
#include<cstdlib>
#include <stdio.h>
#include <string>
#include<boost/asio.hpp>
#include<boost/thread.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/named_scope.hpp>
#define BOOST_LOG_DYN_LINK 1



using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

typedef boost::shared_ptr<tcp::socket> socket_ptr;
typedef boost::shared_ptr<string> string_ptr;
typedef map<socket_ptr, string_ptr> mapOfClients;
typedef boost::shared_ptr<mapOfClients> mapOfClients_ptr;
typedef boost::shared_ptr< list<socket_ptr> > listOfClients_ptr;
typedef boost::shared_ptr< queue<mapOfClients_ptr> > queueOfMessages_ptr;
io_service service;
tcp::acceptor acceptor(service, tcp::endpoint(tcp::v4(), 8001));
boost::mutex mtx;
listOfClients_ptr listOfClients(new list<socket_ptr>);
queueOfMessages_ptr queueOfMessages(new queue<mapOfClients_ptr>);

const int bufSize = 1024;

enum sleepLen
{
	sml = 100,
	lon = 200
};

bool exitCheck(string_ptr);
void disconnectClient(socket_ptr);
void clientConnection();
void outputMessages();
void sendingMessages();

void init()
{
	logging::add_common_attributes();
	auto fmtTimeStamp = expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f"); 
	auto fmtSeverity = expr::attr<logging::trivial::severity_level>("Severity");	   
	auto fsSink = logging::add_file_log	
		(
		keywords::file_name = "test_%Y-%m-%d_%H-%M-%S.%N.log",				   
		keywords::rotation_size = 10 * 1024 * 1024			
		);
	logging::formatter logFmt = expr::format("[%1%] (%2%) %3%")			   
		% fmtTimeStamp % fmtSeverity % expr::smessage;
	fsSink->set_formatter(logFmt);
}

int main(int i_iArgC, char* i_pArgV[])
{
	init();
	boost::thread_group threads;

	threads.create_thread(boost::bind(clientConnection));
	boost::this_thread::sleep(boost::posix_time::millisec(sleepLen::sml));

	threads.create_thread(boost::bind(outputMessages));
	boost::this_thread::sleep(boost::posix_time::millisec(sleepLen::sml));

	threads.create_thread(boost::bind(sendingMessages));
	boost::this_thread::sleep(boost::posix_time::millisec(sleepLen::sml));

	threads.join_all();
	return 0;
}

void clientConnection()
{
	BOOST_LOG_TRIVIAL(debug) << "The server is running";
	cout << "Waiting for clients..." << endl;

	for (;;)
	{
		try {
			socket_ptr clientSock(new tcp::socket(service));
			acceptor.accept(*clientSock);
			BOOST_LOG_TRIVIAL(debug) << "Connecting a new client";
			cout << "New client joined! ";
			BOOST_LOG_TRIVIAL(info) << "New client joined";
			mtx.lock();
			listOfClients->emplace_back(clientSock);
			BOOST_LOG_TRIVIAL(debug) << "New client added to the list";
			mtx.unlock();
		}
		catch (exception &e)
		{
			BOOST_LOG_TRIVIAL(fatal) << "Critical error message - " << e.what();
		}

		cout << listOfClients->size() << " total clients" << endl;
		BOOST_LOG_TRIVIAL(info) << "Total clients - " << listOfClients->size();
	}
}

void outputMessages()
{
	for (;;)
	{

		if (!listOfClients->empty())
		{
			mtx.lock();
			for (auto& clientSock : *listOfClients)
			{
				if (clientSock->available())
				{
					char readBuf[bufSize] = { 0 };
					BOOST_LOG_TRIVIAL(debug) << "Reading a message from a socket";
					int bytesRead = clientSock->read_some(buffer(readBuf, bufSize));

					string_ptr msg(new string(readBuf, bytesRead));

					if (exitCheck(msg))
					{
						disconnectClient(clientSock);
						break;
					}

					mapOfClients_ptr cm(new mapOfClients);
					cm->insert(pair<socket_ptr, string_ptr>(clientSock, msg));
					queueOfMessages->push(cm);
					BOOST_LOG_TRIVIAL(debug) << "Message added to queue";

					cout << "ChatLog: " << *msg << endl;
				}
			}
			mtx.unlock();
		}

		boost::this_thread::sleep(boost::posix_time::millisec(sleepLen::lon));
	}
}

bool exitCheck(string_ptr message)
{
	if (message->find("exit") != string::npos)
		return true;
	else
		return false;
}

void disconnectClient(socket_ptr clientSock)
{
	auto position = find(listOfClients->begin(), listOfClients->end(), clientSock);

	clientSock->shutdown(tcp::socket::shutdown_both);
	clientSock->close();
	BOOST_LOG_TRIVIAL(debug) << "Client's sock closed";
	listOfClients->erase(position);
	BOOST_LOG_TRIVIAL(debug) << "Client deleted from the list";
	cout << "Client Disconnected! " << listOfClients->size() << " total clients" << endl;
	BOOST_LOG_TRIVIAL(info) << "Client disconnected";
}

void sendingMessages()
{
	for (;;)
	{
		if (!queueOfMessages->empty())
		{
			auto message = queueOfMessages->front();

			mtx.lock();
			for (auto& clientSock : *listOfClients)
			{
				BOOST_LOG_TRIVIAL(debug) << "Write message to the socket";
				clientSock->write_some(buffer(*(message->begin()->second), bufSize));
			}
			mtx.unlock();

			mtx.lock();
			queueOfMessages->pop();
			BOOST_LOG_TRIVIAL(debug) << "Message deleted from the queue";
			mtx.unlock();
		}

		boost::this_thread::sleep(boost::posix_time::millisec(sleepLen::lon));
	}
}
