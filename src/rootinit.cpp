#include "rootinit.h"

void remount_root_rw(char* rootfs, char* rootfs_type, unsigned long rootfs_mount_flags) {
	std::cout << "Remounting root as read-write." << std::endl;
	if (mount(rootfs, "/", rootfs_type, rootfs_mount_flags, "") !=0 ) {
		panic("Failed to remount rootfs. Cannot proceed.");
	} else {
		ok("Rootfs has been remounted");
	}

	return;
}

void mount_drive(char* drv, char* fstype, char* destdir, unsigned long flags) {
	std::cout << "Mounting " << drv << " in " << destdir << "..." << std::endl;
	if (mount(drv, destdir, fstype, flags, "") !=0 ) {
		warning("Failed to mount! The system may not behave correctly.");
	} else {
		ok("Drive mounted");
	}

	return;
}

void startup_scripts() {
	std::cout << "Running startup scripts..." << std::endl;

	pid_t pid=fork();

	if (pid==0) {
		static char *argv[]={"busybox", "sh", "/etc/inittab", NULL};
		static char *env[]={"PATH=/bin:/sbin", NULL};
		execve("/bin/busybox", argv, env);
		perror("execve");
		exit(1);
	} else {
		waitpid(pid,0,0);
	}

	return;
}


void launch_programs() {
	std::cout << "Launching programs..." << std::endl;


	tinydir_dir dir;
	tinydir_open(&dir, "/etc/autostart");

	while (dir.has_next) {
		tinydir_file file;
		tinydir_readfile(&dir, &file);

		if (!file.is_dir) {
			char file_name[512] = {0};
			strcat(file_name, "/etc/autostart/");
			strcat(file_name, file.name);
			if (strcmp(file.extension, "unit") == 0) {
				// Executing it as an unit
				std::cout << "Executing unit " << file_name << "..." << std::endl;
				FILE *unit_file = fopen(file_name, "r");
				char *buffer;
				char *lines;
				bool valid = false;
				bool exec = false;
				char *executable;
				char *executable_cmd;
				char *arg;
				bool message = false;
				std::string message_text;
				bool restart = false;
				unsigned long file_size;

				if(unit_file) {
					fseek(unit_file, 0 , SEEK_END);
					file_size = ftell(unit_file);
					fseek(unit_file, 0 , SEEK_SET);

					buffer = new char[file_size];
					lines = new char[file_size];

					char ch;
					int i = 0;
					do {
						ch = fgetc(unit_file);
						buffer[i++] = ch;
					} while (ch != EOF);
					
					i=0;
					lines = strtok(buffer, "\n");
					while (lines != NULL) {
						if(strcmp(lines, "[Unit]") == 0) {
							valid = true;
						} else {
							if (strcmp(lines, "[Exec]") == 0) {
								lines = strtok(NULL,"\n");
								exec = true;
								executable = lines;
							} else if (strcmp(lines, "[Message]") == 0) {
								lines = strtok(NULL,"\n");
								message = true;
								message_text = lines;
							} else if (strcmp(lines, "[Restart]") == 0) {
								lines = strtok(NULL,"\n");
								if (strcmp(lines, "always") == 0) {
									restart = true;
								} else if(strcmp(lines, "never") == 0) {
									restart = false;
								}
							} else if (strcmp(lines, "[Requires]") == 0) {
								warning("[Requires] instruction not yet implemented");
							} 
						}
						lines = strtok(NULL,"\n");
					}

					free(buffer);
					free(lines);
				}

				fclose(unit_file);
				
				if (valid && message)
					std::cout << message_text << std::endl;
				fflush(stdout);
				
				if (valid && exec){
					pid_t controller = fork();
					if (controller == 0) {
						do {
							pid_t daemon = fork();
							if (daemon == 0) {
								char* args[256];
								int i = 1;
								arg = strtok(executable, " ");
								executable_cmd = arg;
								args[0] = executable_cmd;
								while (arg != NULL) {
									arg = strtok(NULL, " ");
									std::cout << arg << std::endl;
									args[i] = arg;
									i++;
								}
								execvp(executable_cmd, args);
								perror("execvp");
								exit(1);
							} else {
								int status;
								waitpid(daemon,&status,0);
								
								int exitcode = WEXITSTATUS(status);
								if(exitcode != 0) {
									warning("Daemon failed!");
									std::cout << "PID " << daemon << " " << executable << " failed with error code " << exitcode << ", not restarting." << std::endl;
									fflush(stdout);
									restart = false;
								}
							}
						} while (restart);

						exit(1);
					}
				} else {
					std::cout << "Unit not valid!" << std::endl;
				}
			} else if (strcmp(file.extension, "disabled") == 0) {
				std::cout << "Ignoring " << file_name << "..." << std::endl;
				// It's disabled, ignore it
			} else if (strcmp(file.extension, "sh") == 0) {
				// Executing it as a shell script
				std::cout << "Executing shell script " << file_name << "..." << std::endl;
				pid_t pid = fork();
				if (pid == 0) {
					execl(file_name, file.name, (char*)NULL);
					perror("execl");
					exit(1);
				} 
			} else {
				// Executing it as a normal executable
				std::cout << "Executing file " << file_name << "..." << std::endl;
				pid_t pid = fork();
				if (pid == 0) {
					execl(file_name, file.name, (char*)NULL);
					perror("execl");
					exit(1);
				}
			}
		}

		tinydir_next(&dir);
	}

	tinydir_close(&dir);	
}