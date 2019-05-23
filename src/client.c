
#include <re.h>
#include "dashperf.h"


#define DEBUG_MODULE "dashperf"
#define DEBUG_LEVEL 6
#include <re_dbg.h>


struct client {
	struct http_cli *cli;
	char *uri;
};


static void destructor(void *data)
{
	struct client *cli = data;

	mem_deref(cli->cli);
	mem_deref(cli->uri);
}


static int handle_hls_playlist(struct client *cli, const struct http_msg *msg)
{
	struct pl pl;

	re_printf("handle hls playlist\n");

	pl_set_mbuf(&pl, msg->mb);

	while (pl.l) {

		const char *end;
		struct pl line;

		end = pl_strchr(&pl, '\n');
		if (!end)
			break;

		line.p = pl.p;
		line.l = end - pl.p;

		re_printf("line = '%r'\n", &line);

		pl_advance(&pl, line.l + 1);
	}

	return 0;
}


static void http_resp_handler(int err, const struct http_msg *msg, void *arg)
{
	struct client *cli = arg;

	re_printf("resp:\n");

	if (err) {
		re_printf("http error: %m\n", err);
		return;
	}

	if (msg->scode <= 199)
		return;
	else if (msg->scode >= 300) {
		re_printf("request failed (%u %r)\n",
			  msg->scode, &msg->reason);
		return;
	}

	re_printf("%H\n", http_msg_print, msg);
	re_printf("%b\n", msg->mb->buf, msg->mb->end);

	if (msg_ctype_cmp(&msg->ctyp, "application", "vnd.apple.mpegurl")) {
		handle_hls_playlist(cli, msg);
	}
	else {
		re_printf("unknown content-type: %r/%r\n",
			  &msg->ctyp.type, &msg->ctyp.subtype);
	}
}


int client_alloc(struct client **clip, struct dnsc *dnsc, const char *uri)
{
	struct client *cli;
	int err;

	cli = mem_zalloc(sizeof(*cli), destructor);

	err = http_client_alloc(&cli->cli, dnsc);
	if (err)
		goto out;

	err = str_dup(&cli->uri, uri);
	if (err)
		goto out;


 out:
	if (err)
		mem_deref(cli);
	else
		*clip = cli;

	return err;
}


int client_start(struct client *cli)
{
	int err;

	re_printf("start: %s\n", cli->uri);

	err = http_request(NULL, cli->cli, "GET", cli->uri,
			   http_resp_handler, NULL, cli, NULL);
	if (err) {
		re_printf("http request failed (%m)\n", err);
		return err;
	}


	return 0;
}


