/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:32:13 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/09 20:00:00 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "Commands.hpp"
#include <csignal>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <cctype>
#include <sstream> // for stringstream

// termination flag set by signal handler in main.cpp
extern volatile sig_atomic_t g_terminate;

Server::Server(const std::string &name, int port, const std::string &pass)
: _listenFd(-1), _name(name), _port(port), _passWord(pass) {}

// Nickname validation function
// Returns true if nickname is valid according to IRC spec
bool Server::isValidNick(const std::string& s) const
{
	// Empty nick is invalid
	if (s.empty())
		return false;

	// First character cannot be special IRC chars
	char c0 = s[0];
	if (c0 == '#' || c0 == ':' || c0 == ' ')
		return false;

	// Check each character in nickname
	for (size_t i = 0; i < s.size(); ++i)
	{
		unsigned char ch = static_cast<unsigned char>(s[i]);

		// Alphanumeric characters are allowed
		if (std::isalnum(ch))
			continue;

		// Special characters allowed in nicknames
		if (ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
		    ch == '\\' || ch == '|' || ch == '-')
			continue;

		// Space is not allowed, any other invalid char fails
		if (ch == ' ')
			return false;
		return false;
	}

	return true;
}

// Check if nickname is already taken by another client
// exceptFd is the FD to skip (usually the client changing nick)
bool Server::isNickTaken(const std::string& s, int exceptFd) const
{
	// Iterate through all connected clients
	for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		// Skip the exception client
		if (it->first == exceptFd)
			continue;

		// Check if this client has the nickname
		const std::string& other = it->second.getNick();
		if (!other.empty() && other == s)
			return true;
	}

	return false;
}

// Send welcome messages to newly registered client
// - Sends RPL_WELCOME (001)
// - Sends RPL_YOURHOST (002)
// - Sends RPL_CREATED (003)
// - Sends RPL_MYINFO (004)
// - Sends MOTD messages
void Server::sendWelcome(Client& cli)
{
	const std::string nick = cli.getNick();

	// Send IRC numeric replies
	sendLine(cli, cli.buildNumeric(_name, 001, nick, "Welcome to the ft_irc server, " + nick));
	sendLine(cli, cli.buildNumeric(_name, 002, nick, "Your host is " + _name));
	sendLine(cli, cli.buildNumeric(_name, 003, nick, "This server was created just now"));
	sendLine(cli, cli.buildNumeric(_name, 004, nick, _name + " ft_irc o o"));

	// Send MOTD
	sendLine(cli, ":" + _name + " 375 " + nick + " :- Message of the Day -");
	sendLine(cli, ":" + _name + " 372 " + nick + " :- Welcome to ft_irc!");
	sendLine(cli, ":" + _name + " 376 " + nick + " :End of /MOTD command.");
}

// Send a line to a client
// - Appends to client's send buffer
// - Sets POLLOUT flag so line will be sent
void Server::sendLine(Client& cli, const std::string& line)
{
	// Add line to client's send buffer
	cli.appendSend(line);

	// Find this client's fd in poll list
	int fd = cli.getFd();
	for (size_t i = 0; i < pfds.size(); ++i)
	{
		if (pfds[i].fd == fd)
		{
			// Enable POLLOUT to trigger sending
			pfds[i].events |= POLLOUT;
			break;
		}
	}
}

void Server::handleMessage(Client& cli, const IRCMessage& m) {
    const std::string cmd = m.command;
    const bool passRequired = !_passWord.empty();

    /* ===== PASS ===== */
    if (cmd == "PASS") {
        if (cli.getRegistered()) {
            sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
            return;
        }
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "PASS :Not enough parameters"));
            return;
        }
        if (m.params[0] == _passWord) {
            cli.setPassOk(true);
        } else {
            cli.setPassOk(false);
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password incorrect"));
            return;
        }
        if (cli.tryFinishRegistration()) sendWelcome(cli);
        return;
    }

    /* ===== NICK ===== */
    if (cmd == "NICK") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 431, "*", "No nickname given"));
            return;
        }
        if (m.params.size() != 1) {
            sendLine(cli, cli.buildNumeric(_name, 432, "*", "Erroneous nickname"));
            return;
        }

        const std::string newNick = m.params[0];

        if (!isValidNick(newNick)) {
            sendLine(cli, ":" + _name + " 432 * " + newNick + " :Erroneous nickname");
            return;
        }
        if (isNickTaken(newNick, cli.getFd())) {
            sendLine(cli, ":" + _name + " 433 * " + newNick + " :Nickname is already in use");
            return;
        }

        if (passRequired && !cli.getPassOk()) {
            cli.setNick(newNick);
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password required before registration"));
            return;
        }

        cli.setNick(newNick);
        if (cli.tryFinishRegistration()) sendWelcome(cli);
        return;
    }

    /* ===== USER ===== */
    if (cmd == "USER") {
        if (cli.getRegistered()) {
            sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
            return;
        }
        if (m.params.size() < 4) {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "USER :Not enough parameters"));
            return;
        }

        const std::string username = m.params[0];
        const std::string realname = m.params[m.params.size() - 1];

        cli.setUser(username, realname);

        if (passRequired && !cli.getPassOk()) {
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password required before registration"));
            return;
        }

        if (cli.tryFinishRegistration()) sendWelcome(cli);
        return;
    }

    /* block non-auth commands until registered */
    if (!cli.getRegistered()) {
        sendLine(cli, cli.buildNumeric(_name, 451, "*", "You have not registered"));
        return;
    }

    /* ===== JOIN ===== */
    if (cmd == "JOIN") {
        cmd_join(this, cli, m);
        return;
    }

    /* ===== PRIVMSG ===== */
    if (cmd == "PRIVMSG") {
        cmd_privmsg(this, cli, m);
        return;
    }

    /* PART removed: not part of mandatory subject */

    /* ===== TOPIC ===== */
    if (cmd == "TOPIC") {
        cmd_topic(this, cli, m);
        return;
    }

    /* ===== INVITE ===== */
    if (cmd == "INVITE") {
        cmd_invite(this, cli, m);
        return;
    }

    /* ===== KICK ===== */
    if (cmd == "KICK") {
        cmd_kick(this, cli, m);
        return;
    }

    /* ===== MODE ===== */
    if (cmd == "MODE") {
        cmd_mode(this, cli, m);
        return;
    }

    /* PING, PONG, QUIT removed: not part of mandatory subject */

    /* not implemented */
    sendLine(cli, cli.buildNumeric(_name, 421, cli.getNick(), cmd + " :Unknown command"));
}

/* INVITE, KICK, MODE, TOPIC moved to separate command handler files */

/* REMOVE CLIENT */
// Remove client from server
// - Removes client from all channels
// - Closes socket
// - Removes from poll fd list
// - Cleans up empty channels
void Server::removeClient(int fd)
{
	// Step 1: Remove client from all channels
	// Also erase empty channels
	for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); )
	{
		// Remove this client from the channel
		it->second.removeClient(fd);

		// If channel is now empty, remove it from the server
		if (it->second.getClients().empty())
		{
			std::map<std::string, Channel>::iterator toErase = it;
			++it;
			_channels.erase(toErase);
		}
		else
		{
			++it;
		}
	}

	// Step 2: Close the client socket
	::close(fd);

	// Step 3: Remove from client map
	_clients.erase(fd);

	// Step 4: Remove from poll fd list
	for (size_t i = 0; i < pfds.size(); ++i)
	{
		if (pfds[i].fd == fd)
		{
			pfds.erase(pfds.begin() + i);
			break;
		}
	}

	std::cout << "Client fd=" << fd << " removed\n";
}

/* RECEIVE DATA */
// Receive data from a client
// - Reads data from socket
// - Parses IRC messages
// - Handles disconnections
void Server::receiveFromClient(int clientFd)
{
	char buffer[4096];
	std::memset(&buffer, 0, sizeof(buffer));

	// Receive data from socket
	ssize_t bytesRead = ::recv(clientFd, buffer, sizeof(buffer), 0);

	// Case 1: Data received
	if (bytesRead > 0)
	{
		Client &cli = _clients[clientFd];
		std::vector<std::string> lines;

		// Parse received data into complete lines
		cli.feed(buffer, static_cast<size_t>(bytesRead), lines);

		// Handle each complete line
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (lines[i].empty())
				continue;

			// Parse IRC message and handle it
			IRCMessage msg = cli.parseLine(lines[i]);
			handleMessage(cli, msg);
		}
	}
	// Case 2: Client disconnected (EOF)
	else if (bytesRead == 0)
	{
		std::cout << "Client fd=" << clientFd << " disconnected" << std::endl;
		removeClient(clientFd);
	}
	// Case 3: Error reading from socket
	else
	{
		std::cerr << "Error reading from client fd=" << clientFd << std::endl;
		removeClient(clientFd);
	}
}

/* ACCEPT NEW CLIENT */
// Accept a new client connection
// - Accepts connection from listening socket
// - Sets socket to non-blocking
// - Creates Client object
// - Adds to poll list
void Server::acceptNewClient(std::vector<struct pollfd> &pfds)
{
	socklen_t cl = sizeof(_claddr);

	// Accept the connection
	int cfd = ::accept(_listenFd, (sockaddr *)&_claddr, &cl);
	if (cfd < 0)
		throw std::runtime_error("accept() failed");

	// Set socket to non-blocking mode
	::fcntl(cfd, F_SETFL, O_NONBLOCK);

	// Get client IP address
	std::string ip = inet_ntoa(_claddr.sin_addr);

	// Create client object and add to map
	_clients[cfd] = Client(cfd, ip);

	// Create poll fd entry for this client
	struct pollfd c;
	c.fd = cfd;
	c.events = POLLIN;
	c.revents = 0;
	pfds.push_back(c);
}

/* RUN SERVER */
// Main server event loop
// - Uses poll() for multiplexed I/O
// - Handles incoming connections
// - Receives/sends data from/to clients
// - Runs until g_terminate is set
void Server::run()
{
	try
	{
		// Add listening socket to poll list
		struct pollfd lp;
		lp.fd = _listenFd;
		lp.events = POLLIN;
		lp.revents = 0;
		pfds.push_back(lp);

		std::cout << "Server listening on fd=" << _listenFd << std::endl;

		// Main event loop - runs until signal received
		while (!g_terminate)
		{
			// Wait for I/O events (timeout = -1 means block indefinitely)
			int ret = ::poll(&pfds[0], pfds.size(), -1);
			if (ret < 0)
			{
				throw std::runtime_error("poll() failed");
			}

			// Process each file descriptor with an event
			for (size_t i = 0; i < pfds.size(); )
			{
				int fd  = pfds[i].fd;
				int rev = pfds[i].revents;
				bool removed = false;

				// Handle error conditions
				if ((rev & POLLERR) || (rev & POLLHUP) || (rev & POLLNVAL))
				{
					if (fd != _listenFd)
					{
						removeClient(fd);
						removed = true;
					}
				}

				// Handle incoming data
				if (!removed && (rev & POLLIN))
				{
					// Listening socket: accept new connection
					if (fd == _listenFd)
					{
						acceptNewClient(pfds);
					}
					// Client socket: receive data
					else
					{
						receiveFromClient(fd);
						// Check if client was removed during processing
						if (_clients.find(fd) == _clients.end())
						{
							removed = true;
						}
					}
				}

				// Handle outgoing data
				if (!removed && (rev & POLLOUT) && fd != _listenFd)
				{
					Client &cli = _clients[fd];

					// Send all pending data from buffer
					while (cli.hasPending())
					{
						const char* data = cli.pendingData();
						size_t      left = cli.pendingSize();
						if (!data || left == 0)
							break;

						// Try to send data
						ssize_t w = ::send(fd, data, left, 0);
						if (w > 0)
						{
							// Mark bytes as sent
							cli.advanceSent(static_cast<size_t>(w));
						}
						else
						{
							// Send error - remove client
							removeClient(fd);
							removed = true;
							break;
						}
					}

					// Disable POLLOUT if no more data to send
					if (!removed && !cli.hasPending())
					{
						pfds[i].events &= ~POLLOUT;
					}
				}

				// Move to next fd (unless we removed current one)
				if (!removed)
					++i;
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error in run(): " << e.what() << std::endl;
	}
}

// Gracefully stop the server
// - Closes listening socket
// - Removes all clients
// - Clears poll fd list
// - Called on signal or error
void Server::stop()
{
	// Step 1: Close listening socket
	if (_listenFd >= 0)
	{
		::close(_listenFd);
		_listenFd = -1;
	}

	// Step 2: Collect all client file descriptors
	std::vector<int> fds;
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		fds.push_back(it->first);
	}

	// Step 3: Remove all clients (closes sockets and cleans up)
	for (size_t i = 0; i < fds.size(); ++i)
	{
		removeClient(fds[i]);
	}

	// Step 4: Clear poll fd vector
	pfds.clear();

	std::cout << "Server stopped cleanly\n";
}

/* INIT SOCKET */
// Initialize listening socket
// - Creates socket
// - Sets socket options (reuse address, non-blocking)
// - Binds to address and port
// - Starts listening
void Server::initSocket()
{
	try
	{
		// Create TCP socket
		_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (_listenFd < 0)
			throw std::runtime_error("socket() failed");

		// Allow reusing address to avoid TIME_WAIT issues
		int yes = 1;
		if (::setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
			throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

		// Set socket to non-blocking mode
		if (::fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
			throw std::runtime_error("fcntl(O_NONBLOCK) failed");

		// Setup server address structure
		struct sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
		addr.sin_port = htons(_port);

		// Bind socket to address and port
		if (::bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
			throw std::runtime_error("bind() failed");

		// Start listening for connections
		if (::listen(_listenFd, SOMAXCONN) < 0)
			throw std::runtime_error("listen() failed");

		std::cout << "Server listening on port " << _port << " (non-blocking)\n";
	}
	catch (const std::exception &e)
	{
		// Cleanup on error
		if (_listenFd >= 0)
		{
			::close(_listenFd);
			_listenFd = -1;
		}
		throw;
	}
}
