/* nutclient.hpp - definitions for nutclient C++ library

   Copyright (C) 2012  Emilien Kia <emilien.kia@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef NUTCLIENT_HPP_SEEN
#define NUTCLIENT_HPP_SEEN

#include <string>
#include <vector>
#include <exception>

namespace nut
{

namespace internal
{
class Socket;
} /* namespace internal */


class Client;
class Device;


/**
 * Basic nut exception.
 */
class NutException : public std::exception
{
public:
	NutException(const std::string& msg):_msg(msg){}
	virtual ~NutException() throw() {}
	virtual const char * what() const throw() {return this->_msg.c_str();}
	virtual std::string str() const throw() {return this->_msg;}
private:
	std::string _msg;
};


/**
 * IO oriented nut exception.
 */
class IOException : public NutException
{
public:
	IOException(const std::string& msg):NutException(msg){}
	virtual ~IOException() throw() {}
private:
	int _errno;
};

/**
 * IO oriented nut exception specialized for unknown host
 */
class UnknownHostException : public IOException
{
public:
	UnknownHostException():IOException("Unknown host"){}
	virtual ~UnknownHostException() throw() {}
};

/**
 * IO oriented nut exception when client is not connected
 */
class NotConnectedException : public IOException
{
public:
	NotConnectedException():IOException("Not connected"){}
	virtual ~NotConnectedException() throw() {}
};

/**
 * A nut client is the starting point to dialog to UPSD.
 * It can connect to an UPSD then retrieve its device list.
 */
class Client
{
	friend class Device;
public:
	Client();
	Client(const std::string& host, int port = 3493)throw(nut::IOException);
	~Client();

	void connect(const std::string& host, int port = 3493)throw(nut::IOException);
	void connect()throw(nut::IOException);

	bool isConnected()const;
	void disconnect();
	
	std::string getHost()const;
	int getPort()const;

	Device getDevice(const std::string& name)throw(NutException);
	std::vector<Device> getDevices()throw(NutException);

protected:
	std::vector<std::string> get(const std::string& subcmd, const std::string& params = "")
		throw(nut::NutException);

	std::vector<std::vector<std::string> > list(const std::string& subcmd, const std::string& params = "")
		throw(nut::NutException);

	std::string sendQuery(const std::string& req)throw(nut::IOException);
	static void detectError(const std::string& req)throw(nut::NutException);

	static std::vector<std::string> explode(const std::string& str, size_t begin=0);

private:
	std::string _host;
	int _port;
	internal::Socket* _socket;
};


/**
 * Device attached to a client.
 * Device is a lightweight class which can be copied easily.
 */
class Device
{
	friend class Client;
public:
	~Device();
	Device(const Device& dev);

	std::string getName()const;
	const Client* getClient()const;
	Client* getClient();

	bool isOk()const;
	operator bool()const;
	bool operator!()const;
	bool operator==(const Device& dev)const;

	std::string getDescription()throw(NutException);

protected:
	Device(Client* client, const std::string& name);

private:
	Client* _client;
	std::string _name;
};


} /* namespace nut */

#endif	/* NUTCLIENT_HPP_SEEN */
