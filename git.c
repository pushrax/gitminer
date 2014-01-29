
#include "git.h"

void run(char *command)
{
	FILE *output;
	output = popen(command, "r");
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer) - 1, output) != NULL)
	{
		printf("%s", buffer);
	}
	pclose(output);
}

void run_with_output(char *command, char *buffer, int len)
{
	FILE *output;
	output = popen(command, "r");
	if (buffer != NULL)
	{
		fgets(buffer, len - 1, output);
	}
	pclose(output);
}

void run_with_input(char *command, char *buffer, int len)
{
	FILE *input;
	input = popen(command, "w");
	fwrite(buffer, sizeof(char), len, input);
	pclose(input);
}

void reset()
{
	printf("[GIT] ");
	run("git reset --hard upstream/master");
}

void add_coin(char *user)
{
	char cmd_buffer[256];
	sprintf(cmd_buffer, "perl -i -pe 's/(%s: )(\\d+)/$1 . ($2+1)/e' LEDGER.txt", user);
	run(cmd_buffer);
	sprintf(cmd_buffer, "grep -q \"%s\" LEDGER.txt || echo \"%s: 1\" >> LEDGER.txt", user, user);
	run(cmd_buffer);
	run("git add LEDGER.txt");
}

void strip_newline(char *buffer)
{
	int len = strlen(buffer);
	buffer[len - 1] = '\0';
}

void commit_body(char *buffer, char *node_id)
{
	char tree[64], parent[64], date[32];
	run_with_output("git write-tree", tree, 64);
	run_with_output("git rev-parse HEAD", parent, 64);
	run_with_output("date +%s", date, 32);
	strip_newline(tree);
	strip_newline(parent);
	strip_newline(date);

	sprintf(buffer,
			"tree %s\n"
			"parent %s\n"
			"author pushrax <jli@shopify.com> %s +0000\n"
			"committer pushrax <jli@shopify.com> %s +0000\n\n"
			"+/u/gitcointip $%s %s\n", tree, parent, date, date, date, node_id);

	int len = strlen(buffer);
	while ((len + 11) % 64)
	{
		buffer[len] = 'a';
		buffer[++len] = '\0';
	}
}

void commit_hash_outputs(char *commit, sha1nfo *s)
{
	char buffer[512];
	int len = strlen(commit);
	sprintf(buffer, "commit %d %s", len + 8, commit);
	len += 11;
	buffer[10] = 0;
	sha1_init(s);
	sha1_write(s, buffer, len);
}

void perform_commit(char *commit, char *sha, int len)
{
	char command[128];
	printf("[GIT] ");
	run_with_input("git hash-object -t commit --stdin -w", commit, len);

	sprintf(command, "git reset --hard \"%s\"", sha);
	run_with_output(command, NULL, 0);
}

void sync_changes(int push)
{
	int need_to_fetch = 0;
	char buffer[1024];
	if (push)
	{
		printf("[GIT] ");
		FILE *output;
		output = popen("git push origin 2>&1", "r");
		while (fgets(buffer, sizeof(buffer) - 1, output) != NULL)
		{
			printf("%s", buffer);
			if (strstr(buffer, "Congratulations") != NULL)
			{
				need_to_fetch = 0;
			}
		}
		pclose(output);
	}
	if (need_to_fetch)
	{
		printf("[GIT] ");
		run("git fetch --all ");
		reset();
	}
}

