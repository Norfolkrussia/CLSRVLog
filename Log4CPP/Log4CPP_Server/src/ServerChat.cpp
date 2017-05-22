
#include<iostream>
#include<list>
#include<map>
#include<queue>
#include<cstdlib>
#include <stdio.h>
#include <string>
#include<boost/asio.hpp>
#include<boost/thread.hpp>
#include <log4cpp/Category.hh>
#include <log4cpp/Appender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/Layout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/Priority.hh>


using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;


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



int main(int i_iArgC, char* i_pArgV[])
{
	log4cpp::Appender *appender1 = new log4cpp::FileAppender("default", "program.log");
	appender1->setLayout(new log4cpp::BasicLayout());	
	log4cpp::Category& root = log4cpp::Category::getRoot();
	root.addAppender(appender1);
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
	root << log4cpp::Priority::DEBUG << "The server is running";
	cout << "Waiting for clients..." << endl;

	for (;;)
	{
		try {
			socket_ptr clientSock(new tcp::socket(service));
			acceptor.accept(*clientSock);
			root << log4cpp::Priority::DEBUG << "Connecting a new client";
			cout << "New client joined! ";
			root << log4cpp::Priority::INFO << "New client joined";
			mtx.lock();
			listOfClients->emplace_back(clientSock);
			root << log4cpp::Priority::DEBUG << "New client added to the list";
			mtx.unlock();
		}
		catch (exception &e)
		{
			root << log4cpp::Priority::ERROR << "Critical error message - " << e.what();
		}

		cout << listOfClients->size() << " total clients" << endl;
		root << log4cpp::Priority::DEBUG << "Total clients - " << listOfClients->size();
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
					root << log4cpp::Priority::DEBUG << "Reading a message from a socket";
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
					root << log4cpp::Priority::DEBUG << "Message added to queue";

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
	root << log4cpp::Priority::DEBUG << "Client's sock closed";
	listOfClients->erase(position);
	root << log4cpp::Priority::DEBUG << "Client deleted from the list";
	cout << "Client Disconnected! " << listOfClients->size() << " total clients" << endl;
	root << log4cpp::Priority::INFO << "Client disconnected";
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
				root << log4cpp::Priority::DEBUG << "Write message to the socket";
				clientSock->write_some(buffer(*(message->begin()->second), bufSize));
			}
			mtx.unlock();

			mtx.lock();
			queueOfMessages->pop();
			root << log4cpp::Priority::DEBUG << "Message deleted from the queue";
			mtx.unlock();
		}

		boost::this_thread::sleep(boost::posix_time::millisec(sleepLen::lon));
	}
}
