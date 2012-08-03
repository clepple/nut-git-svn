/* nutclient.hpp - nutclient C++ library implementation

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

#include "nutclient.hpp"

/* Windows/Linux Socket compatibility layer: */
/* Thanks to Benjamin Roux (http://broux.developpez.com/articles/c/sockets/) */
#ifdef WIN32
#  include <winsock2.h> 
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h> /* close */
#  include <netdb.h> /* gethostbyname */
#  define INVALID_SOCKET -1
#  define SOCKET_ERROR -1
#  define closesocket(s) close(s) 
   typedef int SOCKET;
   typedef struct sockaddr_in SOCKADDR_IN;
   typedef struct sockaddr SOCKADDR;
   typedef struct in_addr IN_ADDR;
#endif /* WIN32 */

/* End of Windows/Linux Socket compatibility layer: */


namespace nut
{

namespace internal
{

/**
 * Internal socket wrapper.
 * Provides only client socket functions.
 * 
 * Implemented as separate internal class to easily hide plateform specificities.
 */
class Socket
{
public:
	Socket();

	void connect(const std::string& host, int port)throw(nut::IOException);
	void disconnect();
	bool isConnected()const;

	size_t read(void* buf, size_t sz)throw(nut::IOException);
	size_t write(const void* buf, size_t sz)throw(nut::IOException);

	std::string read()throw(nut::IOException);
	void write(const std::string& str)throw(nut::IOException);

private:
	SOCKET _sock;
	std::string _buffer; /* Received buffer, string because data should be text only. */
};

Socket::Socket():
_sock(INVALID_SOCKET)
{
}

void Socket::connect(const std::string& host, int port)throw(nut::IOException)
{
	if(_sock != INVALID_SOCKET)
	{
		disconnect();
	}

	// Look for host
	struct hostent *hostinfo = NULL;
	SOCKADDR_IN sin = { 0 };
	hostinfo = ::gethostbyname(host.c_str());
	if(hostinfo == NULL) /* Host doesnt exist */
	{
		throw nut::UnknownHostException();
	}

	// Create socket
	_sock = ::socket(PF_INET, SOCK_STREAM, 0);
	if(_sock == INVALID_SOCKET)
	{
		throw nut::IOException("Cannot create socket");
	}

	// Connect
	sin.sin_addr = *(IN_ADDR *) hostinfo->h_addr;
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	if(::connect(_sock,(SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		_sock = INVALID_SOCKET;
		throw nut::IOException("Cannot connect to host");
	}
}

void Socket::disconnect()
{
	if(_sock != INVALID_SOCKET)
	{
		::closesocket(_sock);
		_sock = INVALID_SOCKET;
	}
	_buffer.clear();
}

bool Socket::isConnected()const
{
	return _sock!=INVALID_SOCKET;
}

size_t Socket::read(void* buf, size_t sz)throw(nut::IOException)
{
	if(!isConnected())
	{
		throw nut::NotConnectedException();
	}

	ssize_t res = ::read(_sock, buf, sz);
	if(res==-1)
	{
		disconnect();
		throw nut::IOException("Error while reading on socket");
	}
	return (size_t) res;
}

size_t Socket::write(const void* buf, size_t sz)throw(nut::IOException)
{
	if(!isConnected())
	{
		throw nut::NotConnectedException();
	}

	ssize_t res = ::write(_sock, buf, sz);
	if(res==-1)
	{
		disconnect();
		throw nut::IOException("Error while writing on socket");
	}
	return (size_t) res;
}

std::string Socket::read()throw(nut::IOException)
{
	std::string res;
	char buff[256];

	while(true)
	{
		// Look at already read data in _buffer
		if(!_buffer.empty())
		{
			size_t idx = _buffer.find('\n');
			if(idx!=std::string::npos)
			{
				res += _buffer.substr(0, idx);
				_buffer.erase(0, idx+1);
				return res;
			}
			res += _buffer;
		}

		// Read new buffer
		size_t sz = read(&buff, 256);
		_buffer.assign(buff, sz);
	}
}

void Socket::write(const std::string& str)throw(nut::IOException)
{
//	write(str.c_str(), str.size());
//	write("\n", 1);
	std::string buff = str + "\n";
	write(buff.c_str(), buff.size());
}

}/* namespace internal */


/*
 *
 * Client implementation
 *
 */


Client::Client():
_host("localhost"),
_port(3493),
_socket(new internal::Socket)
{
	// Do not connect now
}

Client::Client(const std::string& host, int port)throw(IOException):
_socket(new internal::Socket)
{
	connect(host, port);
}

Client::~Client()
{
	delete _socket;
}

void Client::connect(const std::string& host, int port)throw(IOException)
{
	_host = host;
	_port = port;
	connect();
}

void Client::connect()throw(nut::IOException)
{
	_socket->connect(_host, _port);
}

std::string Client::getHost()const
{
	return _host;
}

int Client::getPort()const
{
	return _port;
}

bool Client::isConnected()const
{
	return _socket->isConnected();
}

void Client::disconnect()
{
	_socket->disconnect();
}

std::vector<std::string> Client::get
	(const std::string& subcmd, const std::string& params) throw(NutException)
{
	std::string req = subcmd;
	if(!params.empty())
	{
		req += " " + params;
	}
	std::string res = sendQuery("GET " + req);
	detectError(res);
	if(res.substr(0, req.size()) != req)
	{
		throw NutException("Invalid response");
	}
	
	return explode(res, req.size());
}

std::vector<std::vector<std::string> > Client::list
	(const std::string& subcmd, const std::string& params) throw(NutException)
{
	std::string req = subcmd;
	if(!params.empty())
	{
		req += " " + params;
	}
	std::string res = sendQuery("LIST " + req);
	detectError(res);
	if(res != ("BEGIN LIST " + req))
	{
		throw NutException("Invalid response");
	}

	std::vector<std::vector<std::string> > arr;
	while(true)
	{
		res = _socket->read();
		detectError(res);
		if(res == ("END LIST " + req))
		{
			return arr;
		}
		if(res.substr(0, req.size()) == req)
		{
			arr.push_back(explode(res, req.size()));
		}
		else
		{
			throw NutException("Invalid response");
		}
	}
}

std::string Client::sendQuery(const std::string& req)throw(IOException)
{
	_socket->write(req);
	return _socket->read();
}

void Client::detectError(const std::string& req)throw(NutException)
{
	if(req.substr(0,3)=="ERR")
	{
		throw NutException(req.substr(4));
	}
}

std::vector<std::string> Client::explode(const std::string& str, size_t begin)
{
	std::vector<std::string> res;
	std::string temp;

	enum STATE {
		INIT,
		SIMPLE_STRING,
		QUOTED_STRING,
		SIMPLE_ESCAPE,
		QUOTED_ESCAPE
	} state = INIT;

	for(size_t idx=begin; idx<str.size(); ++idx)
	{
		char c = str[idx];
		switch(state)
		{
		case INIT:
			if(c==' ' /* || c=='\t' */)
			{ /* Do nothing */ }
			else if(c=='"')
			{
				state = QUOTED_STRING;
			}
			else if(c=='\\')
			{
				state = SIMPLE_ESCAPE;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
				state = SIMPLE_STRING;
			}
			break;
		case SIMPLE_STRING:
			if(c==' ' /* || c=='\t' */)
			{
				/* if(!temp.empty()) : Must not occur */
					res.push_back(temp);
				temp.clear();
				state = INIT;
			}
			else if(c=='\\')
			{
				state = SIMPLE_ESCAPE;
			}
			else if(c=='"')
			{
				/* if(!temp.empty()) : Must not occur */
					res.push_back(temp);
				temp.clear();
				state = QUOTED_STRING;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
			}		
			break;
		case QUOTED_STRING:
			if(c=='\\')
			{
				state = QUOTED_ESCAPE;
			}
			else if(c=='"')
			{
				res.push_back(temp);
				temp.clear();
				state = INIT;
			}
			/* What about bad characters ? */
			else
			{
				temp += c;
			}
			break;
		case SIMPLE_ESCAPE:
			if(c=='\\' || c=='"' || c==' ' /* || c=='\t'*/)
			{
				temp += c;
			}
			else
			{
				temp += '\\' + c; // Really do this ?
			}
			state = SIMPLE_STRING;
			break;
		case QUOTED_ESCAPE:
			if(c=='\\' || c=='"')
			{
				temp += c;
			}
			else
			{
				temp += '\\' + c; // Really do this ?
			}
			state = QUOTED_STRING;
			break;
		}
	}

	if(!temp.empty())
	{
		res.push_back(temp);
	}

	return res;
}

Device Client::getDevice(const std::string& name)throw(NutException)
{
	try
	{
		get("UPSDESC", name);
	}
	catch(NutException& ex)
	{
		if(ex.str()=="UNKNOWN-UPS")
			return Device(NULL, "");
		else
			throw;
	}
	return Device(this, name);
}

std::vector<Device> Client::getDevices()throw(NutException)
{
	std::vector<Device> arr;

	std::vector<std::vector<std::string> > devs = list("UPS");
	for(std::vector<std::vector<std::string> >::iterator it=devs.begin();
		it!=devs.end(); ++it)
	{
		std::string id = (*it)[0];
		if(!id.empty())
			arr.push_back(Device(this, id));
	}

	return arr;
}

/*
 *
 * Device implementation
 *
 */

Device::Device(Client* client, const std::string& name):
_client(client),
_name(name)
{
}

Device::Device(const Device& dev):
_client(dev._client),
_name(dev._name)
{
}

Device::~Device()
{
}

std::string Device::getName()const
{
	return _name;
}

const Client* Device::getClient()const
{
	return _client;
}

Client* Device::getClient()
{
	return _client;
}

bool Device::isOk()const
{
	return _client!=NULL && !_name.empty();
}

Device::operator bool()const
{
	return isOk();	
}

bool Device::operator!()const
{
	return !isOk();
}

bool Device::operator==(const Device& dev)const
{
	return dev._client==_client && dev._name==_name;
}

std::string Device::getDescription()throw(NutException)
{
	return getClient()->get("UPSDESC", getName())[0];
}

std::vector<std::string> Device::getVariableValue(const std::string& name)
	throw(NutException)
{
	return getClient()->get("VAR", getName() + " " + name);
}

std::map<std::string,std::vector<std::string> > Device::getVariableValues()
	throw(NutException)
{
	std::map<std::string,std::vector<std::string> >  map;
	
	std::vector<std::vector<std::string> > res = getClient()->list("VAR", getName());
	for(size_t n=0; n<res.size(); ++n)
	{
		std::vector<std::string>& vals = res[n];
		std::string var = vals[0];
		vals.erase(vals.begin());
		map[var] = vals;
	}

	return map;
}

std::set<std::string> Device::getVariableNames()throw(NutException)
{
	std::set<std::string> set;
	
	std::vector<std::vector<std::string> > res = getClient()->list("VAR", getName());
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(res[n][0]);
	}

	return set;
}

std::set<std::string> Device::getRWVariableNames()throw(NutException)
{
	std::set<std::string> set;
	
	std::vector<std::vector<std::string> > res = getClient()->list("RW", getName());
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(res[n][0]);
	}

	return set;
}

Variable Device::getVariable(const std::string& name)throw(NutException)
{
	getClient()->get("VAR", getName() + " " + name);
	return Variable(this, name);
}

std::set<Variable> Device::getVariables()throw(NutException)
{
	std::set<Variable> set;

	std::vector<std::vector<std::string> > res = getClient()->list("VAR", getName());
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(Variable(this, res[n][0]));
	}

	return set;
}

std::set<Variable> Device::getRWVariables()throw(NutException)
{
	std::set<Variable> set;

	std::vector<std::vector<std::string> > res = getClient()->list("RW", getName());
	for(size_t n=0; n<res.size(); ++n)
	{
		set.insert(Variable(this, res[n][0]));
	}

	return set;
}


/*
 *
 * Variable implementation
 *
 */

Variable::Variable(Device* dev, const std::string& name):
_device(dev),
_name(name)
{
}

Variable::Variable(const Variable& var):
_device(var._device),
_name(var._name)
{
}

Variable::~Variable()
{
}

std::string Variable::getName()const
{
	return _name;
}

const Device* Variable::getDevice()const
{
	return _device;
}

Device* Variable::getDevice()
{
	return _device;
}

bool Variable::isOk()const
{
	return _device!=NULL && !_name.empty();

}

Variable::operator bool()const
{
	return isOk();
}

bool Variable::operator!()const
{
	return !isOk();
}

bool Variable::operator==(const Variable& var)const
{
	return var._device==_device && var._name==_name;
}

bool Variable::operator<(const Variable& var)const
{
	return getName()<var.getName();
}

std::vector<std::string> Variable::getValue()throw(NutException)
{
	return getDevice()->getClient()->get("VAR", getDevice()->getName() + " " + getName());
}

std::string Variable::getDescription()throw(NutException)
{
	return getDevice()->getClient()->get("DESC", getDevice()->getName() + " " + getName())[0];
}

} /* namespace nut */

