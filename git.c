
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

void run_with_input(char *command, char *buffer)
{
	FILE *input;
	input = popen(command, "w");
	fputs(buffer, input);
	pclose(input);
}

void clone(char *origin)
{
	char cmd_buffer[256];
	printf("[GIT] ");
	sprintf(cmd_buffer, "git clone %s current-round 2>&1 >/dev/null", origin);
	run(cmd_buffer);
	chdir("current-round");
}

void reset()
{
	printf("[GIT] ");
	run("git reset --hard origin/master");
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

void commit_body(char *buffer)
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
			"+/u/gitcointip $%s verify\n", tree, parent, date, date, date);

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
	//print_n(sha1_result(s), 20);
	//printf("\n%d\n", s->byteCount);
	//exit(0);
}

void perform_commit(char *commit, char *sha)
{
	char command[128];
	printf("[GIT] ");
	run_with_input("git hash-object -t commit --stdin -w", commit);

	sprintf(command, "git reset --hard \"%s\"", sha);
	run_with_output(command, NULL, 0);
}

void sync_changes(int push)
{
	if (push) {
		printf("[GIT] ");
		run("git push");
	}
	printf("[GIT] ");
	run("git fetch --all ");
	reset();
}

