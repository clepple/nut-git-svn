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
#include <map>
#include <set>
#include <exception>

namespace nut
{

namespace internal
{
class Socket;
} /* namespace internal */


class Client;
class Device;
class Variable;
class Command;

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
	friend class Variable;
	friend class Command;
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

	void authenticate(const std::string& user, const std::string& passwd)throw(NutException);
	void logout()throw(NutException);

	Device getDevice(const std::string& name)throw(NutException);
	std::vector<Device> getDevices()throw(NutException);


	std::vector<std::string> get(const std::string& subcmd, const std::string& params = "")
		throw(nut::NutException);

	std::vector<std::vector<std::string> > list(const std::string& subcmd, const std::string& params = "")
		throw(nut::NutException);

protected:
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

	std::vector<std::string> getVariableValue(const std::string& name)throw(NutException);
	std::map<std::string,std::vector<std::string> > getVariableValues()throw(NutException);
	std::set<std::string> getVariableNames()throw(NutException);
	std::set<std::string> getRWVariableNames()throw(NutException);
	void setVariable(const std::string& name, const std::string& value)throw(NutException);

	Variable getVariable(const std::string& name)throw(NutException);
	std::set<Variable> getVariables()throw(NutException);
	std::set<Variable> getRWVariables()throw(NutException);

	std::set<std::string> getCommandNames()throw(NutException);
	std::set<Command> getCommands()throw(NutException);
	Command getCommand(const std::string& name)throw(NutException);
	void executeCommand(const std::string& name)throw(NutException);

	void login()throw(NutException);
	void master()throw(NutException);
	void forcedShutdown()throw(NutException);
	int getNumLogins()throw(NutException);

protected:
	Device(Client* client, const std::string& name);

private:
	Client* _client;
	std::string _name;
};

/**
 * Variable attached to a device.
 * Variable is a lightweight class which can be copied easily.
 */
class Variable
{
	friend class Device;
public:
	~Variable();

	Variable(const Variable& var);

	std::string getName()const;
	const Device* getDevice()const;
	Device* getDevice();

	bool isOk()const;
	operator bool()const;
	bool operator!()const;
	bool operator==(const Variable& var)const;

	bool operator<(const Variable& var)const;

	std::vector<std::string> getValue()throw(NutException);
	std::string getDescription()throw(NutException);

	void setValue(const std::string& value)throw(NutException);

protected:
	Variable(Device* dev, const std::string& name);

private:
	Device* _device;
	std::string _name;
};

/**
 * Command attached to a device.
 * Command is a lightweight class which can be copied easily.
 */
class Command
{
	friend class Device;
public:
	~Command();

	Command(const Command& cmd);

	std::string getName()const;
	const Device* getDevice()const;
	Device* getDevice();

	bool isOk()const;
	operator bool()const;
	bool operator!()const;
	bool operator==(const Command& var)const;

	bool operator<(const Command& var)const;

	std::string getDescription()throw(NutException);

	void execute()throw(NutException);

protected:
	Command(Device* dev, const std::string& name);

private:
	Device* _device;
	std::string _name;
};

} /* namespace nut */

#endif	/* NUTCLIENT_HPP_SEEN */
