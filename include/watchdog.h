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
//#include <exprtk.hpp>

class Datapoint;

/**
 * WatchDog class, derived from Notification BuiltinRule
 */
class WatchDog: public BuiltinRule
{
	public:
		WatchDog() {};
		~WatchDog() {};

		bool	configure(const ConfigCategory& config);
		bool	evalAsset(const Value& assetValue);
		void	lockConfig() { m_configMutex.lock(); };
		void	unlockConfig() { m_configMutex.unlock(); };

		//long	getInterval() { return m_interval; };

	public:
		uint64_t	m_lastCheck;
		long		m_interval;

	private:
		std::mutex	m_configMutex;
};

#endif
