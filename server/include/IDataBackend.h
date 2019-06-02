#pragma once

#ifndef IDATABACKEND_H
#define IDATABACKEND_H

#include <string>

class IDataBackend
{
	public:
		IDataBackend() = default;
		virtual ~IDataBackend() = default;

		virtual int Initialize() = 0;
		virtual void Cleanup() = 0;

		/// Methods for interfacing with the backend.
		// 
		virtual int VerifyAccount(const std::string& account, const std::string& password) = 0;
		virtual int VerifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) = 0;
};

#endif
