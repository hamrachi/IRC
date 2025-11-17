/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/28 19:38:37 by hamrachi          #+#    #+#             */
/*   Updated: 2025/11/17 by hamrachi                ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"
#include <ctime>

// ============================================================================
// CONSTRUCTORS
// ============================================================================

// Default constructor - initializes empty channel
Channel::Channel()
	: _name(""), _topic_time(0), _limit(0), _inviteOnly(false), _topicOnlyOps(false)
{
}

// Constructor with name - initializes channel with given name
Channel::Channel(const std::string& name)
	: _name(name), _topic_time(0), _limit(0), _inviteOnly(false), _topicOnlyOps(false)
{
}

// ============================================================================
// TOPIC MANAGEMENT
// ============================================================================

const std::string& Channel::getName() const
{
	return _name;
}

const std::string& Channel::getTopic() const
{
	return _topic;
}

const std::string& Channel::getTopicWho() const
{
	return _topic_who;
}

time_t Channel::getTopicTime() const
{
	return _topic_time;
}

// Set topic with who set it and current timestamp
void Channel::setTopic(const std::string& t, const std::string& who)
{
	_topic = t;
	_topic_who = who;
	_topic_time = std::time(NULL);
}

// ============================================================================
// CLIENT MANAGEMENT
// ============================================================================

void Channel::addClient(int fd)
{
	_clients.insert(fd);
}

void Channel::removeClient(int fd)
{
	_clients.erase(fd);
	_operators.erase(fd);
}

bool Channel::hasClient(int fd) const
{
	return _clients.count(fd) > 0;
}

const std::set<int>& Channel::getClients() const
{
	return _clients;
}

// ============================================================================
// OPERATOR MANAGEMENT
// ============================================================================

void Channel::addOperator(int fd)
{
	_operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
	_operators.erase(fd);
}

bool Channel::isOperator(int fd) const
{
	return _operators.count(fd) > 0;
}

// ============================================================================
// KEY (PASSWORD) MANAGEMENT
// ============================================================================

void Channel::setKey(const std::string& key)
{
	_key = key;
}

const std::string& Channel::getKey() const
{
	return _key;
}

// ============================================================================
// USER LIMIT MANAGEMENT
// ============================================================================

void Channel::setLimit(size_t l)
{
	_limit = l;
}

size_t Channel::getLimit() const
{
	return _limit;
}

// ============================================================================
// MODE MANAGEMENT: INVITE-ONLY
// ============================================================================

void Channel::setInviteOnly(bool b)
{
	_inviteOnly = b;
}

bool Channel::getInviteOnly() const
{
	return _inviteOnly;
}

// ============================================================================
// MODE MANAGEMENT: TOPIC-ONLY-OPS
// ============================================================================

void Channel::setTopicOnlyOps(bool b)
{
	_topicOnlyOps = b;
}

bool Channel::getTopicOnlyOps() const
{
	return _topicOnlyOps;
}

// ============================================================================
// INVITE LIST MANAGEMENT
// ============================================================================

void Channel::addInvite(int fd)
{
	_invited.insert(fd);
}

void Channel::removeInvite(int fd)
{
	_invited.erase(fd);
}

bool Channel::isInvited(int fd) const
{
	return _invited.count(fd) > 0;
}
