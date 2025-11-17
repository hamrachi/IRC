/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmd_join.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/17 by hamrachi                   #+#    #+#             */
/*   Updated: 2025/11/17 by hamrachi                ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Commands.hpp"
#include "Server.hpp"
#include "Channel.hpp"
#include <sstream>

void cmd_join(Server* server, Client& cli, const IRCMessage& msg) {
    if (msg.params.empty()) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 461, cli.getNick(), "JOIN :Not enough parameters"));
        return;
    }

    std::string chanList = msg.params[0];
    if (chanList.empty()) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 461, cli.getNick(), "JOIN :Not enough parameters"));
        return;
    }

    // Special case: JOIN 0 (leave all channels)
    if (chanList == "0") {
        std::vector<std::string> channelsToLeave;
        for (std::map<std::string, Channel>::iterator it = server->_channels.begin(); it != server->_channels.end(); ++it) {
            if (it->second.hasClient(cli.getFd())) {
                channelsToLeave.push_back(it->first);
            }
        }
        for (size_t i = 0; i < channelsToLeave.size(); ++i) {
            const std::string& chanName = channelsToLeave[i];
            Channel& chan = server->_channels[chanName];
            
            const std::set<int>& members = chan.getClients();
            for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
                server->sendLine(server->_clients[*mit], ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                              " PART " + chanName);
            }
            chan.removeClient(cli.getFd());
            if (chan.getClients().empty()) {
                server->_channels.erase(chanName);
            }
        }
        return;
    }

    // Parse comma-separated channel list and key list
    std::vector<std::string> channels;
    std::vector<std::string> keys;
    
    std::string cur;
    for (size_t i = 0; i < chanList.size(); ++i) {
        if (chanList[i] == ',') {
            if (!cur.empty()) {
                channels.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(chanList[i]);
        }
    }
    if (!cur.empty()) {
        channels.push_back(cur);
    }

    if (msg.params.size() > 1) {
        std::string keyList = msg.params[1];
        cur.clear();
        for (size_t i = 0; i < keyList.size(); ++i) {
            if (keyList[i] == ',') {
                keys.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(keyList[i]);
            }
        }
        keys.push_back(cur);
    }

    // Process each channel
    for (size_t c = 0; c < channels.size(); ++c) {
        std::string chanName = channels[c];
        
        if (!chanName.empty() && chanName[0] != '#' && chanName[0] != '&') {
            chanName = "#" + chanName;
        }

        if (chanName.empty() || (chanName[0] != '#' && chanName[0] != '&')) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 476, cli.getNick(), chanName + " :Bad channel mask"));
            continue;
        }

        bool chanExists = (server->_channels.find(chanName) != server->_channels.end());
        Channel &chan = server->_channels[chanName];
        if (chan.getName().empty()) {
            chan = Channel(chanName);
        }

        if (chan.hasClient(cli.getFd())) {
            continue;
        }

        std::string providedKey;
        if (c < keys.size()) {
            providedKey = keys[c];
        }

        if (!chan.getKey().empty()) {
            if (providedKey != chan.getKey()) {
                server->sendLine(cli, cli.buildNumeric(server->_name, 475, cli.getNick(), chanName + " :Cannot join channel (+k)"));
                continue;
            }
        }

        if (chan.getInviteOnly() && !chan.isInvited(cli.getFd())) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 473, cli.getNick(), chanName + " :Cannot join channel (+i)"));
            continue;
        }

        if (chan.getLimit() > 0 && chan.getClients().size() >= chan.getLimit()) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 471, cli.getNick(), chanName + " :Cannot join channel (+l)"));
            continue;
        }

        if (!chanExists || chan.getClients().empty()) {
            chan.addOperator(cli.getFd());
        }

        chan.addClient(cli.getFd());
        chan.removeInvite(cli.getFd());

        const std::string joinMsg = ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                       " JOIN " + chanName;
        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            server->sendLine(server->_clients[*it], joinMsg);
        }

        if (!chan.getTopic().empty()) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 332, cli.getNick(), chanName + " :" + chan.getTopic()));
            std::ostringstream oss;
            oss << chan.getTopicTime();
            std::string timeStr = oss.str();
            server->sendLine(cli, cli.buildNumeric(server->_name, 333, cli.getNick(), 
                                               chanName + " " + chan.getTopicWho() + " " + timeStr));
        } else {
            server->sendLine(cli, cli.buildNumeric(server->_name, 331, cli.getNick(), chanName + " :No topic is set"));
        }

        std::string names;
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            if (chan.isOperator(*it)) {
                names += "@";
            }
            names += server->_clients[*it].getNick();
            std::set<int>::const_iterator next = it;
            ++next;
            if (next != members.end()) {
                names += " ";
            }
        }
        server->sendLine(cli, cli.buildNumeric(server->_name, 353, cli.getNick(), "= " + chanName + " :" + names));
        server->sendLine(cli, cli.buildNumeric(server->_name, 366, cli.getNick(), chanName + " :End of NAMES list"));
    }
}
