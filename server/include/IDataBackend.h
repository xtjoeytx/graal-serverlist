#pragma once

#ifndef IDATABACKEND_H
#define IDATABACKEND_H

#include <string>
#include "PlayerProfile.h"

class IDataBackend
{
	public:
		IDataBackend() = default;
		virtual ~IDataBackend() = default;

		virtual int Initialize() = 0;
		virtual void Cleanup() = 0;
		virtual int Ping() = 0;

		virtual bool IsConnected() const = 0;
		virtual std::string GetLastError() const = 0;

		/// Methods for interfacing with the backend.
		// 
		virtual bool IsIpBanned(const std::string& ipAddress) = 0;

		virtual int VerifyAccount(const std::string& account, const std::string& password) = 0;
		virtual int VerifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) = 0;

		virtual PlayerProfile GetProfile(const std::string& account) = 0;
		virtual void SetProfile(const PlayerProfile& profile) = 0;
};

#endif
