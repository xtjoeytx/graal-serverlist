#pragma once

#ifndef PLAYERPROFILE_H
#define PLAYERPROFILE_H

#include <string>

class PlayerProfile
{
	public:
		// Constructor
		explicit PlayerProfile(const std::string& accountName)
			: _accountName(accountName), _profileAge(0)
		{
		}

		// Getters
		const std::string& getAccountName() const { return _accountName; }
		const std::string& getName() const { return _profileName; }
		const std::string& getGender() const { return _profileGender; }
		const std::string& getCountry() const { return _profileCountry; }
		const std::string& getEmail() const { return _profileEmail; }
		const std::string& getMessenger() const { return _profileMessenger; }
		const std::string& getWebsite() const { return _profileWebsite; }
		const std::string& getHangout() const { return _profileHangout; }
		const std::string& getQuote() const { return _profileQuote; }
		int getAge() const { return _profileAge; }

		// Setters
		void setAge(int age) { _profileAge = age; }
		void setName(const std::string& name) { _profileName = name; }
		void setGender(const std::string& gender) { _profileGender = gender; }
		void setCountry(const std::string& country) { _profileCountry = country; }
		void setEmail(const std::string& email) { _profileEmail = email; }
		void setMessenger(const std::string& messenger) { _profileMessenger = messenger; }
		void setWebsite(const std::string& website) { _profileWebsite = website; }
		void setHangout(const std::string& hangout) { _profileHangout = hangout; }
		void setQuote(const std::string& quote) { _profileQuote = quote; }

	private:
		std::string _accountName;

		int _profileAge;
		std::string _profileName;
		std::string _profileGender;
		std::string _profileCountry;
		std::string _profileEmail;
		std::string _profileMessenger;
		std::string _profileWebsite;
		std::string _profileHangout;
		std::string _profileQuote;
};

#endif
