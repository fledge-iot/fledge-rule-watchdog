/**
 * Fledge Watch Dog notification rule plugin
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include "watchdog.h"

using namespace std;

/**
 * Configure the rule plugin
 *
 * @param    config	The configuration object to process
 */
bool WatchDog::configure(const ConfigCategory& config)
{
	string assetName =  config.getValue("asset");
	string interval =  config.getValue("interval");
	string source = config.getValue("source");
	string datapoint = config.getValue("datapoint");

	if (assetName.empty())
	{
		Logger::getLogger()->warn("%s Empty values for 'asset'", RULE_NAME);

		// Return true, so it can be configured later
		return true;
	}

	if (interval.empty())
	{
		interval = DEFAULT_INTERVAL;
	}

	// Get configuration lock
	m_configMutex.lock();

	m_source = source;
	m_datapoint = datapoint;

	if (this->hasTriggers())
	{       
		this->removeTriggers();
	}
	this->addTrigger(assetName, NULL);

	// Set interval
	m_interval = atoi(interval.c_str());


	// Release lock
	m_configMutex.unlock();

	return true;
}

/**
 * Rule evaluation:
 * No assets found in input data: return true
 * Found asset with timestamp not in time frame: return true
 * Otherwise: refurn false
 *
 * @param assetValues	JSON input data with:
 * 			- asset data
 * 			- timestamp_${asset} time value
 * 			- _interval asset data
 * 			- timestamp__interval time value
 * Note: _interval and timestamp__interval are always present in inout data
 */
bool WatchDog::evalRule(const string& assetValues)
{
	bool ret = true;

	// Configuration fetch is protected by a lock
	m_configMutex.lock();

	uint64_t interval = m_interval;
	bool firstCheck = m_firstCheck;

	// Check first time run
	if (firstCheck)
	{
		// Next calls will evaluate data
		m_firstCheck = false;

		// Just started, return false
		ret = false;
	}

	// Release lock
	m_configMutex.unlock();

	// Return false if m_lastCheck is not set
	if (!ret)
	{
		return false;
	}

	Document doc;
	doc.Parse(assetValues.c_str());
	if (doc.HasParseError())
	{
		Logger::getLogger()->error("%s plugin_eval(): JSON parse error", RULE_NAME);
		return false;
	}

	bool eval = false;

	// Get "timestamp__interval" value (when rule eval triggered in Notification service)
	double foundTime = 0;
	if (doc.HasMember(TIMESTAMP_INTERVAL_NAME))
	{
		foundTime = doc[TIMESTAMP_INTERVAL_NAME].GetDouble();

		// Set time stamp of rule evaluation
		setEvalTimestamp(foundTime);
	}

	// Iterate throgh all configured assets
	// eval is TRUE if at least one asset has not been found in input data
	// or asset founf but time difference is greater than interval time
	map<std::string, RuleTrigger *>& triggers = getTriggers();
	for (auto & t : triggers)
	{
		string assetName = t.first;
		// Asset found ?
		if (doc.HasMember(assetName.c_str()))
		{
			const Value& asset = doc[assetName.c_str()];
			if (m_source.compare("Readings") || m_datapoint.empty() || asset.HasMember(m_datapoint.c_str()))
			{
				// We are either not looking at readings, no datapoint has been defined or the defined
				// datapoint is in the reading
				string assetTimestamp = "timestamp_" + assetName;

				// Check asset timestamp
				if (doc.HasMember(assetTimestamp.c_str()))
				{
					const Value& assetTime = doc[assetTimestamp.c_str()];
					double timestamp = assetTime.GetDouble();

					// Difference in microseconds:  rule trigger time - asset timestamp
					// Note: time difference as double, then multiply it by 1000
					double diffTime = (foundTime - timestamp) * 1000;

					// Check
					if (diffTime < interval)
					{
						// Asset time is good
						eval = false;
					}
					else
					{
						// Asset timestamp is too old: exit from loop
						eval = true;

						break;
					}
				}
				else
				{
					// No asset timestamp: exit from loop
					eval = true;

					break;
				}
			}
		}
		else
		{
			// Asset not found: exit from loop
			eval = true;

			break;
		}
	}


	// Get configuration lock
	m_configMutex.lock();

	// Set final state
	setState(eval);

	// Release lock
	m_configMutex.unlock();

	// Return rule evaluation
	return eval;
}
