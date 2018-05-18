// mini_server.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"


#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <set>


using boost::asio::ip::tcp;
class session_root
{
public:
	virtual ~session_root() {}
	virtual void disconnect() = 0;
};

typedef std::shared_ptr<session_root> session_root_ptr;

class session_manager
{
public:
	void join(session_root_ptr session)
	{
		sessions.insert(session);
	}
	void remove(session_root_ptr session)
	{
		sessions.erase(session);
		session->disconnect();
	}

private:
	std::set<session_root_ptr> sessions;
};

class session
	: public session_root,
	  public std::enable_shared_from_this<session>
{
public:
	session(tcp::socket socket, session_manager & sessions)
		: socket_(std::move(socket)),sessions_(sessions)
	{
	}

	void start()
	{
		sessions_.join(shared_from_this());
		do_read();
	}

	void disconnect()
	{
	
		socket_.close();
	}

private:
	void do_read()
	{
		socket_.async_read_some(boost::asio::buffer(data_in, max_length),
			[this](boost::system::error_code ec, std::size_t length)
		{
			if (!ec)
			{	
				do_write(length);
			}
			else
			{
				sessions_.remove(shared_from_this());
			}
		});
	}

	void do_write(std::size_t length)
	{
		boost::asio::async_write(socket_, boost::asio::buffer(data_in, length),
			[this](boost::system::error_code ec, std::size_t /*length*/)
		{
			if (!ec)
			{
				do_read();
			}
			else
			{
				sessions_.remove(shared_from_this());
			}
		});
	}

	tcp::socket socket_;
	enum { max_length = 10240 };
	char data_in[max_length];
	char data_out[max_length];
	session_manager & sessions_;

};

class server
{
public:
	server(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
	{
		do_accept();
	}

private:
	void do_accept()
	{
		acceptor_.async_accept(
			[this](boost::system::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				std::shared_ptr<session> ptr = std::make_shared<session>(std::move(socket),sessions);
				ptr->start();

			}

			do_accept();
		});
	}

	tcp::acceptor acceptor_;
	session_manager sessions;
};

int main(int argc, char* argv[])
{
	try
	{

		boost::asio::io_context io_context;

		server s(io_context, 2001);

		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}

