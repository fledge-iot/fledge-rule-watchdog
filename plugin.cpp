/**
 * Fledge Watch Dog notification rule plugin
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include <plugin_api.h>
#include <logger.h>
#include <plugin_exception.h>
#include <iosfwd>
#include <config_category.h>
#include <rapidjson/writer.h>
#include <builtin_rule.h>
#include "version.h"
#include "watchdog.h"

#define DEFAULT_INTERVAL "5000" // In milliseconds

/**
 * Plugin configuration
 *
 * Example:
    {
		"asset": {
			"description": "The asset name for which notifications will be generated.",
			"default": "sinusoid"
		},
		 "interval" : {
		 	"description" : "Rule evalutation interval in milliseconds",
			"default": "10000"
		}
	}
 
 * Expression is composed of datapoint values within given asset name.
 * And if the value of boolean expression toggles, then the notification is sent.
 *
 * NOTE:
 * Datapoint names and values are dynamically added when "plugin_eval" is called
 */


static const char * defaultConfiguration = QUOTE(
{
	"plugin" : {
		"description" : RULE_DESCRIPTION,
		"type" : "string",
		"default" :  RULE_NAME,
		"readonly" : "true"
	},
	"description" : {
		"description" : "Generate a notification if asset data is not present in time interval.",
		"type" : "string",
		"default" : "Generate a notification if asset data is not present in time interval.",
		"displayName" : "Rule",
		"readonly" : "true"
	},
	"source" : {
		"description" : "The source of the data to monitor.",
		"type" : "enumeration",
		"options" : [ "Readings", "Statistics", "Statistics Rate", "Audit"],
		"default" : "Readings",
		"displayName" : "Data Source",
		"order" : "2",
		"mandatory": "true"
	},
	"asset" : {
		"description" : "The name of the asset, statistics or audit code to monitor.",
		"type" : "string",
		"default" : "asset_1",
		"displayName" : "Name",
		"order" : "3",
		"mandatory": "true"
	},
	"datapoint" : {
		"description" : "The name of the datapoint that must exit within the asset, or blank if any datapoint can be used.",
		"type" : "string",
		"default" : "",
		"displayName" : "Datapoint",
		"order" : "4",
		"mandatory": "false",
		"validity" : "source == \"Readings\""
	},
	"interval" : {
		"description" : "Watchdog interval expressed in milliseconds. The rule fires if the defined data is not ibserved within this interval",
		"name" : "interval",
		"type" : "integer",
		"default": DEFAULT_INTERVAL,
		"displayName" : "Watchdog Timer (ms)",
		"order" : "5",
		"mandatory": "true",
		"minimum": "10"
	}
});

using namespace std;

/**
 * The C plugin interface
 */
extern "C" {
/**
 * The C API rule information structure
 */
static PLUGIN_INFORMATION ruleInfo = {
	RULE_NAME,			// Name
	VERSION,			// Version
	0,				// Flags
	PLUGIN_TYPE_NOTIFICATION_RULE,	// Type
	"1.0.0",			// Interface version
	defaultConfiguration		// Configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &ruleInfo;
}

/**
 * Initialise rule objects based in configuration
 *
 * @param    config	The rule configuration category data.
 * @return		The rule handle.
 */
PLUGIN_HANDLE plugin_init(const ConfigCategory& config)
{
	WatchDog* handle = new WatchDog();
	bool rv = handle->configure(config);

	if(rv == false)
	{
		delete handle;
		Logger::getLogger()->info("plugin_init failed");
		handle = NULL;
	}

	return (PLUGIN_HANDLE)handle;
}

/**
 * Free rule resources
 */
void plugin_shutdown(PLUGIN_HANDLE handle)
{
	WatchDog* rule = (WatchDog *)handle;

	// Delete plugin handle
	delete rule;
}

/**
 * Return triggers JSON document
 *
 * @return	JSON string
 */
string plugin_triggers(PLUGIN_HANDLE handle)
{
	string ret;
	WatchDog* rule = (WatchDog *)handle;

	// Configuration fetch is protected by a lock
	rule->lockConfig();

	if (!rule->hasTriggers())
	{
		ret = "{\"triggers\" : []}";

		// Release lock
		rule->unlockConfig();

		return ret;
	}

	ret = "{\"triggers\" : [ ";
	string source = rule->getSource();
	std::map<std::string, RuleTrigger *> triggers = rule->getTriggers();
	for (auto it = triggers.begin();
		  it != triggers.end();
		  ++it)
	{
		if (source.compare("Readings") == 0)
		{
			ret += "{ \"asset\"  : \"" + (*it).first + "\" }";
		}
		else if (source.compare("Statistics") == 0)
		{
			ret += "{ \"statistic\"  : \"" + (*it).first + "\" }";
		}
		else if (source.compare("Statistics Rate") == 0)
		{
			ret += "{ \"statisticRate\"  : \"" + (*it).first + "\" }";
		}
		else if (source.compare("Audit") == 0)
		{
			ret += "{ \"audit\"  : \"" + (*it).first + "\" }";
		}
		else
		{
			Logger::getLogger()->fatal("Incorrect data source defined %s", source.c_str());
		}
		if (std::next(it, 1) != triggers.end())
		{
			ret += ", ";
		}
	}

	ret += " ]";
	// Add Interval object, i.e.  {"Interval" : 1000}
	ret += ", \"interval\" : " + std::to_string(rule->getInterval());
	ret += ", \"evaluate\" : \"any\"";
	ret += "}";

	// Release lock
	rule->unlockConfig();

	Logger::getLogger()->debug("%s plugin_triggers(): ret=%s",
					RULE_NAME,
					ret.c_str());

	return ret;
}

/**
 * Evaluate notification data received
 *
 *  If no assets found or asset time difference is not in time frame,
 *  then return TRUE
 *
 * @param    assetValues	JSON string document
 *				with notification data.
 * @return			True if the rule was triggered,
 *				false otherwise.
 */
bool plugin_eval(PLUGIN_HANDLE handle,
		 const string& assetValues)
{
	WatchDog* rule = (WatchDog *)handle;

	return rule->evalRule(assetValues);
}

/**
 * Return rule trigger reason: trigger or clear the notification. 
 *
 * @return	 A JSON string
 */
string plugin_reason(PLUGIN_HANDLE handle)
{
	WatchDog* rule = (WatchDog *)handle;
	// Get state, assets and timestamp
	BuiltinRule::TriggerInfo info;

	// Configuration fetch is protected by a lock
	rule->lockConfig();

	// Fetch evaluation info
	rule->getFullState(info);

	string ret = "{ \"reason\": \"";
	ret += info.getState() == BuiltinRule::StateTriggered ? "triggered" : "cleared";
	ret += "\"";
	ret += ", \"asset\": " + info.getAssets();
	if (rule->getEvalTimestamp())
	{
		ret += string(", \"timestamp\": \"") + info.getUTCTimestamp() + string("\"");
	}
	ret += " }";

	// Release lock
	rule->unlockConfig();

	Logger::getLogger()->debug("%s plugin_reason(): ret=%s", RULE_NAME, ret.c_str());

	return ret;
}

/**
 * Call the reconfigure method in the plugin
 *
 * Not implemented yet
 *
 * @param    newConfig		The new configuration for the plugin
 */
void plugin_reconfigure(PLUGIN_HANDLE handle,
			const string& newConfig)
{
	WatchDog* rule = (WatchDog *)handle;
	ConfigCategory  config("newCfg", newConfig);
	bool rv = rule->configure(config);

	if(rv == false)
		Logger::getLogger()->error("%s plugin_reconfigure failed",
					RULE_NAME);
}

// End of extern "C"
};
