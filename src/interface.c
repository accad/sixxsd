/**************************************
 SixXSd - SixXS POP Daemon
 by Jeroen Massar <jeroen@sixxs.net>
***************************************
 $Author: jeroen $
 $Id: interface.c,v 1.2 2005-01-29 14:52:31 jeroen Exp $
 $Date: 2005-01-29 14:52:31 $

 SixXSd Interface Management 
**************************************/

#include "sixxsd.h"

bool int_sync(struct sixxs_interface *iface)
{
	return os_sync_interface(iface);
}

bool int_set_endpoint(struct sixxs_interface *iface, struct in_addr ipv4_them)
{
	// Still the same?
	if (memcmp(&iface->ipv4_them, &ipv4_them, sizeof(ipv4_them)) == 0) return true;

	// Change the endpoint
	iface->ipv4_them = ipv4_them;

	iface->state = IFSTATE_UP;

	// Make sure that the kernel thinks the same thing directly
	int_sync(iface);

	return true;
}

// Lame function as we are the AYIYA server so nothing needs
// to be sent to any other programs anyways ;)
bool int_set_port(struct sixxs_interface *iface, unsigned int port)
{
	// Still the same?
	if (iface->ayiya_port == port) return true;
	
	// Change the port
	iface->ayiya_port = port;

	return true;
}

bool int_beat(struct sixxs_interface *iface)
{
	time_t tee = time(NULL);

	// Update the last heartbeat
	iface->hb_lastbeat = mktime(gmtime(&tee));

	return true;
}

struct sixxs_interface *int_get(unsigned int id)
{
	struct sixxs_interface	*iface;
	unsigned int		i;
	
	if (id >= g_conf->max_interfaces)
	{
		dolog(LOG_WARNING, "int_get() - %u out of range (>=%u)\n", id, g_conf->max_interfaces);
		return NULL;
	}
	
	return g_conf->interfaces + id;
}

// Reconfigure (add or update) an interface.
bool int_reconfig(unsigned int id, struct in6_addr *ipv6_us, struct in6_addr *ipv6_them, int prefixlen, struct in_addr ipv4_them, enum iface_type type, enum iface_state state, char *password)
{
	struct sixxs_interface *iface;

	// Get the index
	iface = int_get(id);
	if (!iface) return false;

	// Already configured once ?
	if (iface->type != IFACE_UNSPEC)
	{
		dolog(LOG_INFO, "Reconfiguring interface %u\n", id);

		// Assumptions:
		// - ipv6_them & ipv6_us & prefixlen don't change

		// Changed Type?
		if (type != iface->type)
		{
			iface->type = type;
		}

		// Changed State?
		if (state != iface->state)
		{
			iface->state = state;
		}

		// Changed IPv4 endpoint?
		if (	type == IFACE_PROTO41 &&
			memcmp(&ipv4_them, &iface->ipv4_them, sizeof(ipv4_them)) == 0)
		{
			int_set_endpoint(iface, ipv4_them);
		}

		if (password)
		{
			if (strlen(password) > (sizeof(iface->hb_password)-2))
			{
				dolog(LOG_WARNING, "Trying to set a too long password to interface %u\n", id);
				return false;
			}

			// Changed Password?
			if (	password &&
				strcmp(password, iface->hb_password) != 0)
			{
				SHA_CTX sha1;

				memset(iface->hb_password, 0, sizeof(iface->hb_password));
				memcpy(iface->hb_password, password, strlen(password));

				// Generate a SHA1 of the shared secret
				SHA1_Init(&sha1);
				SHA1_Update(&sha1, iface->hb_password, strlen(iface->hb_password));
				SHA1_Final(iface->ayiya_hash, &sha1);
			}
		}
	}
	else
	{
		iface->interface_id = id;
		iface->type = type;
		iface->state = state;

		memcpy(&iface->ipv4_them,	&ipv4_them,	sizeof(iface->ipv4_them));
		memcpy(&iface->ipv6_them,	ipv6_them,	sizeof(iface->ipv6_them));
		memcpy(&iface->ipv6_us,		ipv6_us,	sizeof(iface->ipv6_us));

		if (password)
		{
			SHA_CTX sha1;

			memset(iface->hb_password, 0, sizeof(iface->hb_password));
			memcpy(&iface->hb_password, password, strlen(password));

			// Generate a SHA1 of the shared secret
			SHA1_Init(&sha1);
			SHA1_Update(&sha1, iface->hb_password, strlen(iface->hb_password));
			SHA1_Final(iface->ayiya_hash, &sha1);
		}

		// Construct the devicename
		snprintf(iface->name, sizeof(iface->name), "%s%u", g_conf->pop_tunneldevice, id);

		dolog(LOG_INFO, "Initializing new interface %u: %s, type=%s, state=%s\n",
			id,
			iface->name,
			(type == IFACE_UNSPEC		? "invalid" :
			type == IFACE_IGNORE		? "ignore" :
			type == IFACE_PROTO41		? "proto-41" :
			type == IFACE_PROTO41_HB	? "proto-41-HB" :
			type == IFACE_TINC		? "tinc" :
			type == IFACE_AYIYA		? "AYIYA" : "?"),

			(state == IFSTATE_DISABLED	? "disabled" :
			state == IFSTATE_UP		? "up" :
			state == IFSTATE_DOWN		? "down" : "?")
			);

		// Start up the device
		if (iface->type == IFACE_AYIYA)
		{
			ayiya_init(iface);
		}
		
		// Local address
		pfx_reconfig(ipv6_us, 128, NULL, true, true, id);

		// Remote address
		pfx_reconfig(ipv6_them, 128, ipv6_us, true, true, id);
	}

	return true;
}
