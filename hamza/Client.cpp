/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/28 19:38:11 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/07 01:52:59 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include <ctime>
#include <iostream>
#include <cctype> 
#include <sstream>

Client::Client(int fd, const std::string &ip) : _fd(fd), _ip(ip), _nick(""), _user(""), _realname(""),
                                                _passOk(false), _hasNick(false), _hasUser(false), _registered(false), _recvBuf("") ,_sendBuf("") ,_sendOff(0) {}



void Client::appendSend(const std::string& line) {
    // Store with CRLF so poll-out can send raw bytes
    _sendBuf += line;
    _sendBuf += "\r\n";
}
bool Client::hasPending() const {
    return !_sendBuf.empty();
}

const char* Client::pendingData() const {
    if (_sendOff < _sendBuf.size())
        return _sendBuf.c_str() + _sendOff;
    return 0;
}

size_t Client::pendingSize() const {
    if (_sendOff < _sendBuf.size())
        return _sendBuf.size() - _sendOff;
    return 0;
}

void Client::advanceSent(size_t n) {
    _sendOff += n;
    if (_sendOff >= _sendBuf.size()) {
        // everything sent → reset
        _sendBuf.clear();
        _sendOff = 0;
    }
}



std::string Client::buildNumeric(const std::string& serverName, int code, const std::string& target, const std::string& text)
{
    std::ostringstream oss;

    // prefix with server name
    oss << ":" << serverName << " ";

    // numeric code always 3 digits (e.g. 462)
    if (code < 10)      
        oss << "00" << code;
    else if (code < 100) 
        oss << "0" << code;
    else                 
        oss << code;

    // target (nickname or "*")
    oss << " " << (target.empty() ? "*" : target);

    // actual message text
    oss << " :" << text;

    return oss.str();
}

bool Client::tryFinishRegistration() {
    if (_registered) {
        return false; // already registered
    }

    // need PASS ok + NICK + USER before registration is complete
    if (!_passOk)   return false;
    if (!_hasNick)  return false;
    if (!_hasUser)  return false;

    _registered = true;
    return true;
}



static std::string toUpperCopy(const std::string& s) {
    std::string r = s;
    size_t i = 0;
    while (i < r.size()) 
    {
        r[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[i])));
        ++i;
    }
    return r;
}

IRCMessage Client::parseLine(const std::string &line)
{
    IRCMessage msg;
    std::vector<std::string> tokens;

    std::string cur;
    bool trailing = false;

    size_t i = 0;
    while (i < line.size())
    {
        char c = line[i];

        if (!trailing)
        {
            if (c == ' ')
                {
                    if (!cur.empty())
                    {
                        tokens.push_back(cur);
                        cur.clear();
                    }
                    
                    size_t j = i + 1;
                    
                    while (j < line.size() && line[j] == ' ')
                        ++j;
                     i = j;
                    continue;
                }
            if (c == ':' && !tokens.empty())
                {
                    if (!cur.empty())
                    {
                     tokens.push_back(cur);
                        cur.clear();
                    }
                    if (i + 1 < line.size())
                        tokens.push_back(line.substr(i + 1));
                    trailing = true;
                    break;
                }
            cur.push_back(c);
            ++i;
        }
        else
        {
                break;
        }
    }

    if (!cur.empty())
    {
        tokens.push_back(cur);
        cur.clear();
    }

    if (!tokens.empty())
    {
        msg.command = toUpperCopy(tokens[0]);
        size_t k = 1;
        while (k < tokens.size())
        {
            msg.params.push_back(tokens[k]);
            ++k;
        }
    }
    return msg;
}

void Client::feed(const char *data, size_t n, std::vector<std::string> &outLines)
{
    if (!n)
        return;

    // 1) append new bytes
    printf("data from feed: %s\n", data);
    _recvBuf.append(data, n);

    // 2) normalize line endings:
    //    - turn CRLF -> LF
    //    - turn lone CR -> LF
    // This lets us split on '\n' only in the next step.
    for (std::string::size_type i = 0; i < _recvBuf.size();)
    {
        if (_recvBuf[i] == '\r')
        {
            if (i + 1 < _recvBuf.size() && _recvBuf[i + 1] == '\n')
            {
                // CRLF -> make it single LF
                _recvBuf[i] = '\n';
                _recvBuf.erase(i + 1, 1);
            }
            else
            {
                // lone CR -> make it LF
                _recvBuf[i] = '\n';
                ++i;
            }
        }
        else
        {
            ++i;
        }
    }

    // 3) split on '\n'
    std::string::size_type nl = _recvBuf.find('\n');
    while (nl != std::string::npos)
    {
        std::string line = _recvBuf.substr(0, nl);
        _recvBuf.erase(0, nl + 1);

        if (!line.empty())
            outLines.push_back(line);

        nl = _recvBuf.find('\n');
    }
    for (size_t i = 0; i < outLines.size(); ++i)
    {
        std::cout << "[FEED OUT] line " << i
                  << ": \"" << outLines[i] << "\"" << std::endl;
    }
}
