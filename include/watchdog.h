#ifndef _WATCHDOG_RULE_H
#define _WATCHDOG_RULE_H
/*
 * Fledge WatchDog notification rule class
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */
#include <plugin.h>
#include <config_category.h>
#include <rule_plugin.h>
#include <builtin_rule.h>

#define RULE_NAME "WatchDog"
#define RULE_DESCRIPTION  "Generate a notification based on the last time of received data"
#define DEFAULT_INTERVAL "5000" // In milliseconds
#define TIMESTAMP_INTERVAL_NAME "timestamp__interval"

/**
 * WatchDog class, derived from Notification BuiltinRule
 */
class WatchDog: public BuiltinRule
{
	public:
		WatchDog() {
			m_firstCheck = true;
		};
		~WatchDog() {};

		bool	configure(const ConfigCategory& config);
		bool	evalAsset(const Value& assetValue);
		void	lockConfig() { m_configMutex.lock(); };
		void	unlockConfig() { m_configMutex.unlock(); };
		long	getInterval() { return m_interval; };
		bool	evalRule(const std::string& assetValues);

	private:
		std::mutex	m_configMutex;
		bool	m_firstCheck;
		long	m_interval;
};

#endif
