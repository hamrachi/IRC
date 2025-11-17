/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmd_mode.cpp                                       :+:      :+:    :+:   */
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
#include <cstdio>
#include <cstdlib>

void cmd_mode(Server* server, Client& cli, const IRCMessage& msg) {
    if (msg.params.empty()) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 461, cli.getNick(), "MODE :Not enough parameters"));
        return;
    }

    std::string target = msg.params[0];

    if (target[0] == '#' || target[0] == '&') {
        std::map<std::string, Channel>::iterator it = server->_channels.find(target);
        if (it == server->_channels.end()) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 403, cli.getNick(), target + " :No such channel"));
            return;
        }

        Channel &chan = it->second;

        if (!chan.hasClient(cli.getFd())) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 442, cli.getNick(), target + " :You're not on that channel"));
            return;
        }

        if (msg.params.size() == 1) {
            std::string modeStr;
            if (chan.getInviteOnly()) modeStr += 'i';
            if (chan.getTopicOnlyOps()) modeStr += 't';
            if (!chan.getKey().empty()) modeStr += 'k';
            if (chan.getLimit() > 0) modeStr += 'l';

            std::string params;
            if (!chan.getKey().empty()) { params += chan.getKey(); params += " "; }
            if (chan.getLimit() > 0) {
                char buf[16]; std::sprintf(buf, "%d", static_cast<int>(chan.getLimit()));
                params += buf;
            }
            if (!params.empty() && params[params.size()-1] == ' ') params.erase(params.size()-1);

            if (params.empty()) {
                server->sendLine(cli, cli.buildNumeric(server->_name, 324, cli.getNick(), target + " +" + modeStr));
            } else {
                server->sendLine(cli, cli.buildNumeric(server->_name, 324, cli.getNick(), target + " +" + modeStr + " " + params));
            }
            return;
        }

        if (!chan.isOperator(cli.getFd())) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 482, cli.getNick(), target + " :You're not channel operator"));
            return;
        }

        if (msg.params.size() < 2) return;
        std::string modes = msg.params[1];

        bool add = true;
        size_t paramIdx = 2;

        for (size_t i = 0; i < modes.size(); ++i) {
            char c = modes[i];
            if (c == '+') { add = true; }
            else if (c == '-') { add = false; }
            else if (c == 'i') {
                chan.setInviteOnly(add);
            }
            else if (c == 't') {
                chan.setTopicOnlyOps(add);
            }
            else if (c == 'o') {
                if (paramIdx >= msg.params.size()) continue;
                std::string nick = msg.params[paramIdx++];
                int targetFd = -1;
                for (std::map<int, Client>::iterator it = server->_clients.begin(); it != server->_clients.end(); ++it) {
                    if (it->second.getNick() == nick) {
                        targetFd = it->first;
                        break;
                    }
                }
                if (targetFd > 0 && chan.hasClient(targetFd)) {
                    if (add) chan.addOperator(targetFd);
                    else chan.removeOperator(targetFd);
                }
            }
            else if (c == 'k') {
                if (paramIdx >= msg.params.size()) continue;
                std::string key = msg.params[paramIdx++];
                if (add) chan.setKey(key);
                else chan.setKey("");
            }
            else if (c == 'l') {
                if (add) {
                    if (paramIdx >= msg.params.size()) continue;
                    int limit = std::atoi(msg.params[paramIdx++].c_str());
                    chan.setLimit(limit);
                } else {
                    chan.setLimit(0);
                }
            }
        }

        const std::set<int>& members = chan.getClients();
        std::string modeStr;
        add = true;
        paramIdx = 2;
        for (size_t i = 0; i < modes.size(); ++i) {
            char c = modes[i];
            if (c == '+') {
                if (!modeStr.empty()) server->sendLine(cli, "SEND_MODE_PLACEHOLDER");
                modeStr = "+";
                add = true;
            }
            else if (c == '-') {
                if (!modeStr.empty()) server->sendLine(cli, "SEND_MODE_PLACEHOLDER");
                modeStr = "-";
                add = false;
            }
            else if (c == 'i' || c == 't' || c == 'o' || c == 'k' || c == 'l') {
                modeStr += c;
                if (c == 'o' && paramIdx < msg.params.size()) paramIdx++;
                else if (c == 'k' && paramIdx < msg.params.size()) paramIdx++;
                else if (c == 'l' && add && paramIdx < msg.params.size()) paramIdx++;
            }
        }

        for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
            Client &member = server->_clients[*mit];
            server->sendLine(member, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                 " MODE " + target + " " + modes);
        }
        return;
    }

    if (target == cli.getNick()) {
        if (msg.params.size() == 1) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 221, cli.getNick(), "+"));
            return;
        }
    }

    server->sendLine(cli, cli.buildNumeric(server->_name, 403, cli.getNick(), target + " :No such channel"));
}
