/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 19:01:02 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/04 21:01:31 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <deque>
#include <ctime>

struct IRCMessage {
    std::string command;
    std::vector<std::string> params;
};

class Client {
private:
    int _fd;
    std::string _ip;
    std::string _nick;
    std::string _user;
    std::string _realname;
    
    bool _passOk;
    bool _hasNick;
    bool _hasUser;
    bool _registered;

    std::string _recvBuf;

public:
    Client(int fd = -1, const std::string& ip = "");

    // Getters
    int getFd() const { return _fd; }
    const std::string& getIp() const { return _ip; }
    const std::string& getNick() const { return _nick; }
    bool getRegistered() const { return _registered; }

    // Setters
    void setNick(const std::string& n) { _nick = n; _hasNick = true; }
    void setUser(const std::string& u, const std::string& real) { _user = u; _realname = real; _hasUser = true; }
    void setPassOk(bool ok) { _passOk = ok; }
    void setRegistered(bool r) { _registered = r; }
    void feed(const char* data, size_t n, std::vector<std::string>& outLines);
    IRCMessage parseLine(const std::string &line);
};

#endif
