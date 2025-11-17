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
#include <ctime>

class Channel {
private:
    std::string _name;
    std::set<int> _clients;        // client FDs
    std::set<int> _operators;      // client FDs with operator privileges
    std::set<int> _invited;        // client FDs invited to the channel
    std::string _topic;
    std::string _topic_who;        // nickname of who set the topic
    time_t _topic_time;            // timestamp when topic was set
    std::string _key;              // password
    size_t _limit;                 // user limit
    bool _inviteOnly;
    bool _topicOnlyOps;

public:
    Channel();
    Channel(const std::string& name);

    const std::string& getName() const;
    const std::string& getTopic() const;
    const std::string& getTopicWho() const;
    time_t getTopicTime() const;
    void setTopic(const std::string& t, const std::string& who);

    void addClient(int fd);
    void removeClient(int fd);
    bool hasClient(int fd) const;
    const std::set<int>& getClients() const;

    void addOperator(int fd);
    void removeOperator(int fd);
    bool isOperator(int fd) const;

    void setKey(const std::string& key);
    const std::string& getKey() const;

    void setLimit(size_t l);
    size_t getLimit() const;

    void setInviteOnly(bool b);
    bool getInviteOnly() const;

    void setTopicOnlyOps(bool b);
    bool getTopicOnlyOps() const;

    void addInvite(int fd);
    void removeInvite(int fd);
    bool isInvited(int fd) const;
};

#endif