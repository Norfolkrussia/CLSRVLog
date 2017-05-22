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
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP
#define _DEBUG

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

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



int main(int argc, char** argv)
{
	el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToStandardOutput, "false");
	el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Format, "%level %datetime{%H:%m:%s}: %msg");
	try
	{
		boost::thread_group threads;
		socket_ptr sock(new tcp::socket(service));

		string_ptr signature(creatingSignature());
		signatureCpy = signature;
		LOG(DEBUG) << "Connecting to the server";
		sock->connect(ep);
		LOG(DEBUG) << "Connected to the server";
		cout << "Welcome to the ChatServer\nType \"exit\" to quit" << endl;

		threads.create_thread(boost::bind(outputMessages, sock));
		threads.create_thread(boost::bind(readingMessages, sock, signature));
		threads.create_thread(boost::bind(sendingMessages, sock, signature));

		threads.join_all();
	}
	catch (std::exception& e)
	{
		LOG(FATAL) << "Critical error message #%d" << e.what();
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
			LOG(DEBUG) << "Reading a message from a socket";
			bytesRead = sock->read_some(buffer(readBuf, inputSize));
			string_ptr msg(new string(readBuf, bytesRead));

			queueOfMessages->push(msg);
			LOG(DEBUG) << "Message added to queue";
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
			LOG(DEBUG) << "Write message to the socket";
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
			LOG(DEBUG) << "Message deleted from the queue";
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
