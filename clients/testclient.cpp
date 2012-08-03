/* testclient.cpp - nutclient C++ library test program

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
using namespace nut;

#include <iostream>
#include <string>
using namespace std;


int main(int, char**)
{
	cout << "testclient" << endl;

	string host = "localhost";
	int port = 3493;

	try
	{
		Client client;
		client.connect(host, port);

		cout << "connected to " << host << ":" << port << endl;

		std::vector<Device> devs = client.getDevices();
		for(auto dev : devs)
		{
			try
			{
				cout << dev.getName() << " : " << dev.getDescription() << endl;

				std::set<Variable> vars = dev.getVariables();
				for(auto var : vars)
				{
					cout << "  " << var.getName() << " : " << var.getValue()[0]
						<< " (" << var.getDescription() << ")" << endl;
				}
			}
			catch(NutException& ex)
			{
				if(ex.str()=="DRIVER-NOT-CONNECTED")
				{
					cerr << dev.getName() << " : not connected" << endl;
				}
				else
				{
					throw;
				}
			}
		}

	}
	catch(NutException& ex)
	{
		cerr << "Exception : '" << ex.what() << "'" << endl;
	}

	return 0;
}


