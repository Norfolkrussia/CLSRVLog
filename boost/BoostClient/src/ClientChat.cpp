// ClientChaaaaaaat.cpp : Defines the entry point for the console application.
//

#include<iostream>
#include<queue>
#include<string>
#include<cstdlib>
#include<boost/thread.hpp>
#include<boost/bind.hpp>
#include<boost/asio.hpp>
#include<boost/asio/ip/tcp.hpp>
#include<boost/algorithm/string.hpp>
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
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

typedef boost::shared_ptr<tcp::socket> socket_ptr;
typedef boost::shared_ptr<string> string_ptr;
typedef boost::shared_ptr< queue<string_ptr> > queueOfMessages_ptr;

io_service service;
queueOfMessages_ptr queueOfMessages(new queue<string_ptr>);
tcp::endpoint ep(ip::address::from_string("127.0.0.1"), 8001);
const int inputSize = 256;
string_ptr signatureCpy;

bool checkinOwnMessage(string_ptr);
void outputMessages(socket_ptr);
void readingMessages(socket_ptr, string_ptr);
void sendingMessages(socket_ptr, string_ptr);
string* creatingSignature();


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
	try
	{
		boost::thread_group threads;
		socket_ptr sock(new tcp::socket(service));

		string_ptr signature(creatingSignature());
		signatureCpy = signature;
		BOOST_LOG_TRIVIAL(debug) << "Connecting to the server";
		sock->connect(ep);
		BOOST_LOG_TRIVIAL(debug) << "Connected to the server";
		cout << "Welcome to the ChatServer\nType \"exit\" to quit" << endl;

		threads.create_thread(boost::bind(outputMessages, sock));
		threads.create_thread(boost::bind(readingMessages, sock, signature));
		threads.create_thread(boost::bind(sendingMessages, sock, signature));

		threads.join_all();
	}
	catch (std::exception& e)
	{
		BOOST_LOG_TRIVIAL(debug) << "Critical error message #%d" << e.what();
	}

	return 0;
}

string* creatingSignature()
{
	const int inputSize = 256;
	char inputBuf[inputSize] = { 0 };
	char nameBuf[inputSize] = { 0 };
	string* signature = new string(": ");

	cout << "Please input a new username: ";
	cin.getline(nameBuf, inputSize);
	*signature = (string)nameBuf + *signature;
	boost::algorithm::to_lower(*signature);

	return signature;
}

void readingMessages(socket_ptr sock, string_ptr signature)
{
	int bytesRead = 0;
	char readBuf[1024] = { 0 };

	for (;;)
	{
		if (sock->available())
		{
			BOOST_LOG_TRIVIAL(debug) << "Reading a message from a socket";
			bytesRead = sock->read_some(buffer(readBuf, inputSize));
			string_ptr msg(new string(readBuf, bytesRead));

			queueOfMessages->push(msg);
			BOOST_LOG_TRIVIAL(debug) << "Message added to queue";
		}

		boost::this_thread::sleep(boost::posix_time::millisec(1000));
	}
}

void sendingMessages(socket_ptr sock, string_ptr signature)
{
	char inputBuf[inputSize] = { 0 };
	string inputMsg;

	for (;;)
	{
		cin.getline(inputBuf, inputSize);
		inputMsg = *signature + (string)inputBuf + '\n';

		if (!inputMsg.empty())
		{
			BOOST_LOG_TRIVIAL(debug) << "Write message to the socket";
			sock->write_some(buffer(inputMsg, inputSize));
		}

		if (inputMsg.find("exit") != string::npos)
			exit(1);

		inputMsg.clear();
		memset(inputBuf, 0, inputSize);
	}
}

void outputMessages(socket_ptr sock)
{
	for (;;)
	{
		if (!queueOfMessages->empty())
		{
			if (!checkinOwnMessage(queueOfMessages->front()))
			{
				cout << "\n" + *(queueOfMessages->front());
			}

			queueOfMessages->pop();
			BOOST_LOG_TRIVIAL(debug) << "Message deleted from the queue";
		}

		boost::this_thread::sleep(boost::posix_time::millisec(1000));
	}
}

bool checkinOwnMessage(string_ptr message)
{
	if (message->find(*signatureCpy) != string::npos)
		return true;
	else
		return false;
}
