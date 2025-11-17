/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Commands.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/17 by hamrachi                   #+#    #+#             */
/*   Updated: 2025/11/17 by hamrachi                ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include "Server.hpp"
#include "Client.hpp"
#include <string>

// Command handler function signatures
void cmd_join(Server* server, Client& cli, const IRCMessage& msg);
void cmd_privmsg(Server* server, Client& cli, const IRCMessage& msg);
void cmd_mode(Server* server, Client& cli, const IRCMessage& msg);
void cmd_topic(Server* server, Client& cli, const IRCMessage& msg);
void cmd_invite(Server* server, Client& cli, const IRCMessage& msg);
void cmd_kick(Server* server, Client& cli, const IRCMessage& msg);

#endif
