/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:32:13 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/09 00:48:58 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <arpa/inet.h> //inet_ntoa 

Server::Server(const std::string &name, int port, const std::string &pass) : _listenFd(-1), _name(name),
 _port(port), _passWord(pass){
             // _listenFd;
             // _port;
                            };

// Allow at least: alnum, []{}\| and '-' ; forbid leading '#', ':', and space.
// (This satisfies the subject: servers MUST allow the listed set; we allow a bit extra like '-')
bool Server::isValidNick(const std::string& s) const {
    if (s.empty()) return false;

    // no leading chantype ('#'), no leading ':', no space
    char c0 = s[0];
    if (c0 == '#' || c0 == ':' || c0 == ' ') return false;

    size_t i = 0;
    while (i < s.size()) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (std::isalnum(ch)) { ++i; continue; }
        if (ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == '\\' || ch == '|' || ch == '-') {
            ++i; continue;
        }
        // forbid ASCII space
        if (ch == ' ') return false;
        // anything else → invalid
        return false;
    }
    return true;
}

bool Server::isNickTaken(const std::string& s, int exceptFd) const {
    std::map<int, Client>::const_iterator it = _clients.begin();
    while (it != _clients.end()) {
        if (it->first != exceptFd) {
            const std::string& other = it->second.getNick();
            if (!other.empty() && other == s) {
                return true;
            }
        }
        ++it;
    }
    return false;
}

            
void Server::sendWelcome(Client& cli) 
{
    const std::string nick = cli.getNick();

    // 001 RPL_WELCOME
    sendLine(cli, cli.buildNumeric(_name, 001, nick,
        "Welcome to the ft_irc server, " + nick));

    // 002 RPL_YOURHOST
    sendLine(cli, cli.buildNumeric(_name, 002, nick,
        "Your host is " + _name));

    // 003 RPL_CREATED
    sendLine(cli, cli.buildNumeric(_name, 003, nick,
        "This server was created just now"));

    // 004 RPL_MYINFO
    sendLine(cli, cli.buildNumeric(_name, 004, nick,
        _name + " ft_irc o o"));

    // ---- Minimal MOTD (subject requires MOTD or ERR_NOMOTD) ----
    sendLine(cli, ":" + _name + " 375 " + nick + " :- Message of the Day -");
    sendLine(cli, ":" + _name + " 372 " + nick + " :- Welcome to ft_irc!");
    sendLine(cli, ":" + _name + " 376 " + nick + " :End of /MOTD command.");
}



void Server::sendLine(Client& cli, const std::string& line) 
{
    cli.appendSend(line);

    int fd = cli.getFd();
    size_t i = 0;
    while (i < pfds.size()) 
    {
        if (pfds[i].fd == fd) 
        {
            pfds[i].events |= POLLOUT;
            break;
        }
        ++i;
    }
}

void Server::handleMessage(Client& cli, const IRCMessage& m) {
    
    const std::string cmd = m.command;
    const bool passRequired = !_passWord.empty();

    // ===== PASS =====
    if (cmd == "PASS") {
    // Already registered? --> 462
    if (cli.getRegistered()) {
        sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
        return;
    }

    // Need param --> 461
    if (m.params.empty()) {
        sendLine(cli, cli.buildNumeric(_name, 461, "*", "PASS :Not enough parameters"));
        return;
    }

    // Only the LAST PASS before registration is used:
    // set passOk based on the latest value (true if matches, false if not)
    if (m.params[0] == _passWord) {
        cli.setPassOk(true);
    } else {
        cli.setPassOk(false);
        // Wrong password --> 464 (do NOT close; allow retry per subject)
        sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password incorrect"));
        return;
    }

    // If NICK+USER were already provided, this may finish registration
    if (cli.tryFinishRegistration()) {
        sendWelcome(cli);
    }
    return;
}

// helper: is password required by server?

   // ===== NICK =====
// 
if (cmd == "NICK") {
    // 431: no parameter
    if (m.params.empty()) {
        sendLine(cli, cli.buildNumeric(_name, 431, "*", "No nickname given"));
        return;
    }
if (m.params.size() != 1) {
    // too many or too few params
    sendLine(cli, cli.buildNumeric(_name, 432, "*", "Erroneous nickname"));
    return;
}

    const std::string newNick = m.params[0];

    // 432: invalid nickname
    if (!isValidNick(newNick)) {
        sendLine(cli, ":" + _name + " 432 * " + newNick + " :Erroneous nickname");
        return;
    }

    // 433: already in use
    if (isNickTaken(newNick, cli.getFd())) {
        sendLine(cli, ":" + _name + " 433 * " + newNick + " :Nickname is already in use");
        return;
    }

    // === check PASS requirement ===
    const bool passRequired = !_passWord.empty();
    if (passRequired && !cli.getPassOk()) {
        // accept the nick but don’t allow registration yet
        cli.setNick(newNick);
        sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password required before registration"));
        return;
    }

    // accept the nick
    cli.setNick(newNick);

    // if all conditions are now OK → finish registration
    if (cli.tryFinishRegistration()) {
        sendWelcome(cli);
    }
    return;
}


    // ===== USER =====
// ===== USER =====
if (cmd == "USER") {
    // 462: already registered
    if (cli.getRegistered()) {
        sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
        return;
    }

    // Need at least 4 params: <username> <mode/0> <unused/*> :<realname>
    if (m.params.size() < 4) {
        sendLine(cli, cli.buildNumeric(_name, 461, "*", "USER :Not enough parameters"));
        return;
    }

    // Extract username and realname.
    // parseLine already merges trailing into a single last param,
    // so realname is params.back().
    const std::string username = m.params[0];
    const std::string realname = m.params[m.params.size() - 1];

    // Store fields (do not force-check m.params[1], m.params[2]; they are SHOULD)
    cli.setUser(username, realname);

    // If a server password is configured, do not complete registration until PASS is OK
    
    if (passRequired && !cli.getPassOk()) {
        // Optional hint; keeps behavior friendly without violating MUSTs
        sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password required before registration"));
        return;
    }

    // Try to finish registration if PASS + NICK + USER are all set
    if (cli.tryFinishRegistration()) {
        sendWelcome(cli); // 001..004 + 005 and MOTD/422
    }
    return;
}


    // ===== block non-auth commands until registered =====
    if (!cli.getRegistered()) {
        sendLine(cli, cli.buildNumeric(_name, 451, "*", "You have not registered"));
        return;
    }

    // ===== JOIN =====
    if (cmd == "JOIN") {
        // TODO: joinChannel(cli, m.params);
        return;
    }

    // ===== PRIVMSG =====
    if (cmd == "PRIVMSG") {
        // TODO: privmsg(cli, m.params);
        return;
    }

    // ===== KICK / INVITE / TOPIC / MODE =====
    if (cmd == "KICK") {
        // TODO
        return;
    }
    if (cmd == "INVITE") {
        // TODO
        return;
    }
    if (cmd == "TOPIC") {
        // TODO
        return;
    }
    if (cmd == "MODE") {
        // TODO
        return;
    }

    // ===== Unknown =====
    sendLine(cli, cli.buildNumeric(_name, 421, cli.getNick(), cmd + " :Unknown command"));
}


void Server::removeClient(int fd) {
    ::close(fd);
    _clients.erase(fd); // remove from map
    
    for (size_t i = 0; i < pfds.size(); ++i) { //remove frome pfds
        if (pfds[i].fd == fd) {
            pfds.erase(pfds.begin() + i);
            break;
        }
    }
    std::cout << "Client fd=" << fd << " removed\n";
}


// void Server::receiveFromClient(int clientFd)
// {
//     char buffer[4000];
//     ssize_t bytesRead = ::recv(clientFd, buffer, sizeof(buffer) - 1, 0);

//     if (bytesRead < 0)
//     {
//         std::cerr << "Error reading from client fd=" << clientFd << std::endl;
//         ::close(clientFd);
//     }
//     else if (bytesRead == 0)
//     {
//         std::cout << "Client fd=" << clientFd << " disconnected" << std::endl;
//         ::close(clientFd);
//     }
//     else
//     {
//         buffer[bytesRead] = '\0';
//         std::cout << "Message from fd=" << clientFd << ": " << buffer << std::endl;
//     }
// }

// Server.cpp

void Server::receiveFromClient(int clientFd)
{
    char buffer[4096];
    memset(&buffer, 0 , sizeof(buffer)); // أكبر شوية باش نقلّلو عدد الـrecv
    ssize_t bytesRead = ::recv(clientFd, buffer, sizeof(buffer), 0);

    if (bytesRead > 0)
    {
        Client &cli = _clients[clientFd];
        std::vector<std::string> lines;
        printf("this id buffer  %s\n ", buffer);
        cli.feed(buffer, static_cast<size_t>(bytesRead), lines);
    
        for (size_t i = 0; i < lines.size(); ++i) 
        {
            if (lines[i].empty()) 
                continue; // طنّش الفارغ
            //std::cout << "[RAW fd=" << clientFd << "] " << lines[i] << std::endl;
            IRCMessage msg = cli.parseLine(lines[i]);
            // Print command
                //std::cout << "[PARSE] command = " << msg.command << std::endl;

                // Print params
            size_t p = 0;
            while (p < msg.params.size())  
            {
                //std::cout << "[PARSE] param[" << p << "] = " << msg.params[p] << std::endl;
                ++p;
            }
            handleMessage(cli,msg);

            
            // ↓ لاحقاً: غادي نديرو parse + handleMessage(...)
            // IRCMessage msg = Client::parseLine(lines[i]);
            // handleMessage(cli, msg);
        }
    }
    else if (bytesRead == 0)
    {
        std::cout << "Client fd=" << clientFd << " disconnected" << std::endl;
        removeClient(clientFd);
    }
    else // bytesRead < 0
    {
        std::cerr << "Error reading from client fd=" << clientFd << std::endl;
        removeClient(clientFd);
    }
}



void Server::acceptNewClient(std::vector<struct pollfd> &pfds) {
    
    socklen_t cl = sizeof(_claddr);

    int cfd = ::accept(_listenFd, (sockaddr *)&_claddr, &cl);
    if (cfd < 0)
        throw std::runtime_error("accept() failed");

    ::fcntl(cfd, F_SETFL, O_NONBLOCK);

    std::string ip = inet_ntoa(_claddr.sin_addr);   // "192.168.1.5"  ip of client 

    _clients[cfd] = Client(cfd, ip);

    // std::cout << "new client: fd=" << cfd << " ip=" << ip << std::endl;

    struct pollfd c;
    c.fd = cfd;
    c.events = POLLIN;
    c.revents = 0;
    pfds.push_back(c);
}


void Server::run()
{
    try
    {
        // add the listening socket to poll list
        struct pollfd lp;
        lp.fd = _listenFd;
        lp.events = POLLIN;
        lp.revents = 0;
        pfds.push_back(lp);

        std::cout << "Server listening on fd=" << _listenFd << std::endl;

        while (true)
        {
            int ret = ::poll(&pfds[0], pfds.size(), -1);
            if (ret < 0)
            {
                throw std::runtime_error("poll() failed");
            }

            size_t i = 0;
            while (i < pfds.size())
            {
                int fd  = pfds[i].fd;
                int rev = pfds[i].revents;
                bool removed = false;

                // === 1) error / hangup → remove client
                if ((rev & POLLERR) || (rev & POLLHUP) || (rev & POLLNVAL)) {
                    if (fd != _listenFd) 
                    {
                        removeClient(fd);
                        removed = true;
                    }
                }

                // === 2) readable?
                if (!removed && (rev & POLLIN)) 
                {
                    if (fd == _listenFd)
                    {
                        // new connection
                        
                        acceptNewClient(pfds);
                    }
                    else
                    {
                        // client sent data
                        receiveFromClient(fd);
                        if (_clients.find(fd) == _clients.end()) 
                        {
                            removed = true; // client was removed inside
                        }
                    }
                }

                // === 3) writable? flush outgoing buffer
                if (!removed && (rev & POLLOUT) && fd != _listenFd) {
                    Client &cli = _clients[fd];

                    while (cli.hasPending()) 
                    {
                        const char* data = cli.pendingData();
                        size_t      left = cli.pendingSize();
                        if (!data || left == 0) 
                            break;

                        ssize_t w = ::send(fd, data, left, 0);
                        if (w > 0) 
                        {
                            cli.advanceSent(static_cast<size_t>(w));
                        } 
                        else 
                        {
                            removeClient(fd);
                            removed = true;
                            break;
                        }
                    }

                    if (!removed && !cli.hasPending()) {
                        pfds[i].events &= ~POLLOUT;
                    }
                }

                if (!removed) 
                {
                    ++i; // move to next fd
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in run(): " << e.what() << std::endl;
    }

};

void Server::initSocket()
{
    try
    {
        _listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_listenFd < 0)
            throw std::runtime_error("socket() failed");

        int yes = 1;
        if (::setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
            throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

        if (::fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
            throw std::runtime_error("fcntl(O_NONBLOCK) failed");

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to any address
        addr.sin_port = htons(_port);

        if (::bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed");

        if (::listen(_listenFd, SOMAXCONN) < 0)
            throw std::runtime_error("listen() failed");

        std::cout << "Server listening on port " << _port << " (non-blocking)\n";
    }
    catch (const std::exception &e)
    {
        if (_listenFd >= 0)
        {
            ::close(_listenFd);
            _listenFd = -1;
        }
        throw;
    }
};
