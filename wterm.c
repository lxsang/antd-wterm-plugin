#define PLUGIN_IMPLEMENT 1
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <string.h>
#include <antd/plugin.h>

typedef struct{
	int fdm;
	pid_t pid;
} wterm_rq_t;

void init()
{
}
void destroy()
{
}

void *process(void *data)
{
	char buff[BUFFLEN];
	int max_fdm, status;
	fd_set fd_in;
	antd_request_t *rq = (antd_request_t *)data;
	wterm_rq_t* wterm_data = (wterm_rq_t*)dvalue(rq->request, "WTERM_DATA");
	ws_msg_header_t *h = NULL;
	antd_task_t *task = NULL;
	void *cl = (void *)rq->client;
	int cl_fd = ((antd_client_t *)cl)->sock;

	task = antd_create_task(NULL, (void *)rq, NULL, time(NULL));
	task->priority++;

	if(!wterm_data)
	{
		return task;
	}

	int fdm = wterm_data->fdm;
	pid_t pid = wterm_data->pid;
	// first we need to verify if child exits
	// if it is the case then quit the session 
	// and return an empty task
	pid_t wpid = waitpid(pid, &status, WNOHANG);
	if(wpid == -1 || wpid > 0)
	{
		// child exits
		LOG("Child process finished\n");
		return task;
	}
	struct timeval timeout;      
	timeout.tv_sec = 0;
	timeout.tv_usec = 500;
	// otherwise, check if data available

	FD_ZERO(&fd_in);
	//FD_SET(0, &fd_in);
	FD_SET(fdm, &fd_in);
	FD_SET(cl_fd, &fd_in);
	max_fdm = fdm > cl_fd ? fdm : cl_fd;
	int rc = select(max_fdm + 1, &fd_in, NULL, NULL, &timeout);
	switch (rc)
	{
	case -1:
		LOG("Error %d on select()\n", errno);
		ws_close(cl, 1011);
		return task;
	case 0:
		// time out
		goto wait_for_child;
		break;
	// we have data
	default:
	{
		// If data is on websocket side
		if (FD_ISSET(cl_fd, &fd_in))
		{
			//LOG("data from client\n");
			h = ws_read_header(cl);
			if (h)
			{
				if (h->mask == 0)
				{
					LOG("%s\n", "Data is not mask");
					// kill the child process
					kill(pid, SIGKILL);
					free(h);
					ws_close(cl, 1011);
					goto wait_for_child;
				}
				if (h->opcode == WS_CLOSE)
				{
					LOG("%s\n", "Websocket: connection closed");
					ws_close(cl, 1011);
					kill(pid, SIGKILL);
					free(h);
					goto wait_for_child;
				}
				else if (h->opcode == WS_TEXT)
				{
					int l;
					char *tok = NULL;
					char *tok1 = NULL;
					char *orgs = NULL;
					while ((l = ws_read_data(cl, h, sizeof(buff), (uint8_t*)buff)) > 0)
					{
						char c = buff[0];
						switch (c)
						{
						case 'i': // input from user
							write(fdm, buff + 1, l - 1);
							break;

						case 's': // terminal resize event
							buff[l] = '\0';
							tok = strdup(buff + 1);
							orgs = tok;
							tok1 = strsep(&tok, ":");
							if (tok != NULL && tok1 != NULL)
							{
								int cols = atoi(tok1);
								int rows = atoi(tok);
								//free(tok);
								struct winsize win = {0, 0, 0, 0};
								if (ioctl(fdm, TIOCGWINSZ, &win) != 0)
								{
									if (errno != EINVAL)
									{
										break;
									}
									memset(&win, 0, sizeof(win));
								}
								//printf("Setting winsize\n");
								if (rows >= 0)
									win.ws_row = rows;
								if (cols >= 0)
									win.ws_col = cols;

								if (ioctl(fdm, TIOCSWINSZ, (char *)&win) != 0)
									printf("Cannot set winsize\n");

								free(orgs);
							}

							break;

						default:
							break;
						}
						//ws_t(cl,buff);
					}
					/*if(l == -1)
									{
										printf("EXIT FROM CLIENT \n");
				   						write(fdm, "exit\n", 5);
				      					return;
									}*/
				}

				free(h);
			}
			else
			{
				kill(pid, SIGKILL);
				ws_close(cl, 1000);
			}
		}
		// If data on master side of PTY
		if (FD_ISSET(fdm, &fd_in))
		{
			//LOG("data from server\n");
			//rc = read(fdm, buff, sizeof(buff));
			if ((rc = read(fdm, buff, sizeof(buff) - 1)) > 0)
			{
				// Send data to websocket
				buff[rc] = '\0';
				ws_t(cl, buff);
			}
			else
			{
				if (rc < 0)
				{
					LOG("Error %d on read standard input. Exit now\n", errno);
					kill(pid, SIGKILL);
					ws_close(cl, 1011);
					goto wait_for_child;
				}
			}
		}
		//printf("DONE\n");
	}
	} // End switch
	wait_for_child:
	task->handle = process;
	task->type = HEAVY;
	task->access_time = time(NULL);
	return task;
}

void *handle(void *rqdata)
{
	antd_request_t *rq = (antd_request_t *)rqdata;
	antd_task_t *task = antd_create_task(NULL, (void *)rq, NULL, time(NULL));
	task->priority++;
	void *cl = (void *)rq->client;
	if (ws_enable(rq->request))
	{
		int fdm, fds;
		pid_t pid;
		// Check arguments
		fdm = posix_openpt(O_RDWR);
		if (fdm < 0)
		{
			LOG("Error %d on posix_openpt()\n", errno);
			ws_close(cl, 1011);
			return task;
		}

		int rc = grantpt(fdm);
		if (rc != 0)
		{
			LOG("Error %d on grantpt()\n", errno);
			ws_close(cl, 1011);
			return task;
		}

		rc = unlockpt(fdm);
		if (rc != 0)
		{
			LOG("Error %d on unlockpt()\n", errno);
			ws_close(cl, 1011);
			return task;
		}

		// Open the slave side ot the PTY
		fds = open(ptsname(fdm), O_RDWR);

		// Create the child process
		pid = fork();
		if (pid)
		{
			free(task);
			wterm_rq_t* wdata = (wterm_rq_t*)malloc(sizeof(*wdata));
			dput(rq->request,"WTERM_DATA", wdata);
			wdata->fdm = fdm;
			wdata->pid = pid;
			task = antd_create_task(process, (void*)rq ,NULL, time(NULL));
			task->priority++;
			task->type = HEAVY;
			return task;
		}
		else
		{
			//struct termios slave_orig_term_settings; // Saved terminal settings
			//struct termios new_term_settings; // Current terminal settings

			// CHILD

			// Close the master side of the PTY
			close(fdm);

			// Save the defaults parameters of the slave side of the PTY
			//rc = tcgetattr(fds, &slave_orig_term_settings);

			// Set RAW mode on slave side of PTY
			//new_term_settings = slave_orig_term_settings;
			//cfmakeraw (&new_term_settings);
			//tcsetattr (fds, TCSANOW, &new_term_settings);

			// The slave side of the PTY becomes the standard input and outputs of the child process
			// we use cook mode here
			close(0); // Close standard input (current terminal)
			close(1); // Close standard output (current terminal)
			close(2); // Close standard error (current terminal)

			dup(fds); // PTY becomes standard input (0)
			dup(fds); // PTY becomes standard output (1)
			dup(fds); // PTY becomes standard error (2)

			// Now the original file descriptor is useless
			close(fds);

			// Make the current process a new session leader
			setsid();

			// As the child is a session leader, set the controlling terminal to be the slave side of the PTY
			// (Mandatory for programs like the shell to make them manage correctly their outputs)
			ioctl(0, TIOCSCTTY, 1);

			//system("/bin/bash");
			system("TERM=linux sudo login");
			//LOG("%s\n","Terminal exit");
			_exit(1);
		}
	}
	return task;
}