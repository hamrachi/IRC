/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/28 19:38:45 by hamrachi          #+#    #+#             */
/*   Updated: 2025/08/30 16:54:31 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>

class Channel {
private:
    std::string _name;
    std::set<int> _clients;        // client FDs
    std::set<int> _operators;      // client FDs with operator privileges
    std::set<int> _invited;        // client FDs invited to the channel
    std::string _topic;
    std::string _key;              // password
    size_t _limit;                 // user limit
    bool _inviteOnly;
    bool _topicOnlyOps;

public:
    Channel() : _name(""), _limit(0), _inviteOnly(false), _topicOnlyOps(false) {}
    Channel(const std::string& name) : _name(name), _limit(0), _inviteOnly(false), _topicOnlyOps(false) {}

    const std::string& getName() const { return _name; }
    const std::string& getTopic() const { return _topic; }
    void setTopic(const std::string& t) { _topic = t; }

    void addClient(int fd) { _clients.insert(fd); }
    void removeClient(int fd) { _clients.erase(fd); _operators.erase(fd); }
    bool hasClient(int fd) const { return _clients.count(fd); }
    const std::set<int>& getClients() const { return _clients; }

    void addOperator(int fd) { _operators.insert(fd); }
    void removeOperator(int fd) { _operators.erase(fd); }
    bool isOperator(int fd) const { return _operators.count(fd); }

    void setKey(const std::string& key) { _key = key; }
    const std::string& getKey() const { return _key; }

    void setLimit(size_t l) { _limit = l; }
    size_t getLimit() const { return _limit; }

    void setInviteOnly(bool b) { _inviteOnly = b; }
    bool getInviteOnly() const { return _inviteOnly; }

    void setTopicOnlyOps(bool b) { _topicOnlyOps = b; }
    bool getTopicOnlyOps() const { return _topicOnlyOps; }

    void addInvite(int fd) { _invited.insert(fd); }
    void removeInvite(int fd) { _invited.erase(fd); }
    bool isInvited(int fd) const { return _invited.count(fd); }
};

#endif