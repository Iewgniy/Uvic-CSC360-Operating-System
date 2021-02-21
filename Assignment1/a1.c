
#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h> 
#include <readline/history.h>

#define SIZE256 256



typedef struct process_node{
    pid_t pid;
	char dir[SIZE256];
    char name[SIZE256]; 
    struct process_node *next;
}process_node;
process_node* node_head=NULL;
int node_number = 0;
int task_id = 1;
int in = 0;
// check the terminated process
void check_zombieProcess(void)
{
    // in+=1;
    // printf("%d",node_number);
	if(node_number > 0){
		// find the terminated process
		pid_t ter = waitpid(0, NULL, WNOHANG);
		//if ter>0 return by waitpid, the process is terminated;
		while(ter > 0){
			
			process_node* curNode = node_head;
			if(node_head->pid == ter){
				printf("%d: %s %s has terminated.\n", node_head->pid, node_head->dir, node_head->name);
				node_head = node_head->next;
			}else{
				while(curNode->next->pid != ter){
					curNode= curNode->next;		
				}
				printf("%d: %s %s has terminated.\n", curNode->next->pid, curNode->next->dir, curNode->next->name);
				curNode->next = curNode->next->next;
				//curNode= curNode->next;

			}
			// find the next of terminated process
			ter = waitpid(0, NULL, WNOHANG);
			//if ter>0 return by waitpid, the process is terminated;
			node_number = node_number-1;
		}
	}

    // int status;
	// int retVal = 0;
	
	// while(1) {
	// 	usleep(1000);
	// 	if(headPnode == NULL){
	// 		return ;
	// 	}
	// 	retVal = waitpid(-1, &status, WNOHANG);
	// 	if(retVal > 0) {
	// 		//remove the background process from your data structure
	// 	}
	// 	else if(retVal == 0){
	// 		break;
	// 	}
	// 	else{
	// 		perror("waitpid failed");
	// 		exit(EXIT_FAILURE);
	// 	}
	// }
	// return ;

}

//set first node to NULL
int main()
{
    // store the command list for arguments
	char* strCmd[SIZE256];
    char* strInput;
	char* parInput;
	char curDir[SIZE256];
    getcwd(curDir, sizeof(curDir));
	pid_t ter;

    while(1)
    {
        int inputId = 0; 

		// check the terminated process and display

		// show the promter
        // printf("%s","000");
		check_zombieProcess();
        // printf("%s","111");
		// get the input string
		strInput = readline(" ");
        //strInput = "bg ls";
		if(strlen(strInput)>0)
		{
		// parse the input string, the limit char is " " or "\n"
				
		  parInput = strtok(strInput, " ");
		  //if strInput is not null split each str in to strCmd[]

		  for(int i=0; i<100; i++){
                if(parInput==NULL){
                    break;
                }
                else{
                    strCmd[inputId++] = parInput;
                    parInput = strtok(NULL, " ");
                    //printf("token: %s\n", parInput);
                }
		    }
				
		strCmd[inputId] = NULL;
        }
        //if user didn't input things or input null
        else{
            printf("Error: Empty Command\n");
        }
        //bg
        if(strcmp(strCmd[0], "bg") == 0){
			// the command for "bg" - background 
			ter = fork();

			if(ter == 0){
				// child process
				
				// show cmd content in next line
				execvp(strCmd[1], strCmd+1);
				//exit(1);
			}else if(ter > 0) {
				// parent process
				
				process_node* addNode;
				addNode = (process_node*)malloc(sizeof(process_node));
				addNode->pid = ter;
				
 				//addNode->num = node_number;
				// addNode->num = task_id++;
				// save the filename
				strcpy(addNode->name, strCmd[1]);
				//printf("%s\n", strCmd[1]);
				//save dictory
				strcpy(addNode->dir, curDir);
				node_number++;
			
				if(node_number == 1){
					// add to the head
					node_head = addNode;
					// there is only one node, put in the linkedlist
				}else{
					// add the node to the tail
					process_node* node_next = node_head;
					// make new node;
					while(node_next->next != NULL){
						// if no more node, jump out loop
						node_next = node_next->next;
						// connext new node to list
					}
					node_next->next = addNode;
					//make the tail node be null
				}
				addNode->next = NULL;
			}else{
				printf("Error: command (bg) - add node to the process.\n");
			}
       }
       //bglist
       else if(strcmp(strCmd[0], "bglist") == 0){
			// the command for "bglist"
			process_node* curNode = node_head;
			while(curNode != NULL){
				
				printf("%d: %s/%s \n", curNode->pid, curNode->dir, curNode->name);
				//show next node inf 

				curNode = curNode->next;
				//go to next node
			}
			printf("Total Background jobs: %d\n", node_number);
       }
       //bgkill
       else if(strcmp(strCmd[0], "bgkill") == 0){
			// the command for "bgkill" for kill the process
		    ter = atoi(strCmd[1]);
			if(kill(ter, SIGKILL)){
				// Error to kill process
				printf("Error: command (bgkill %d) - Failed \n", ter); 
			}else{
                node_number--;
            }
       }
        //bgstop
        else if(strcmp(strCmd[0], "bgstop") == 0){
			// the command for "bgstop" for stop the process temporayliy
		    ter = atoi(strCmd[1]);
			if(kill(ter, SIGSTOP)){
				// Error to stop process
				printf("Error: Process (%d) does not exist. \n", ter); 
			}
        }
        //bgstart
        else if(strcmp(strCmd[0], "bgstart") == 0){
			// the command for "bgstart" for restart the process
		    ter = atoi(strCmd[1]);
			if(kill(ter, SIGCONT)){
				// Error to restart process
				printf("Error: Process (%d) does not exist. \n", ter); 
			}
        }
        //pstat
        else if(strcmp(strCmd[0], "pstat") == 0){
		    // ter = atoi(strCmd[1]);
			// char labels_1[SIZE256];
            
            FILE* file1;
            FILE* file2; 
            int index[5]={1,2,13,14,23};
            char* label1[5];
            int i=0;
            // char labels_2[SIZE256];
            char file1_name[SIZE256];
            char file2_name[SIZE256];
            char* buffer1;
            char* buffer2;
            char* token;
            // strcpy(file1_name,"/prco/");
            // strcat(file1_name,strCmd[1]);
            // strcat(file1_name,"/stat");
            // strcpy(file2_name,"/prco/");
            // strcat(file2_name,strCmd[1]);
            // strcat(file2_name,"/status");
            sprintf(file1_name,"%s%s%s","/proc/",strCmd[1],"/stat");
            sprintf(file1_name,"%s%s%s","/proc/",strCmd[1],"/status");
            file1 = fopen(file1_name,"r");
            file2 = fopen(file2_name,"r");
            //read from file1.
            fgets(buffer1,SIZE256,file1);
            for(int j=0;j<5;j++){
                while(index[j]!=i){
                    if(i==0){
                        token = strtok(buffer1, "\t");
                        i++;
                    }else{
                        token = strtok(NULL, "\t");
                        i++;
                    }
                }
                label1[j]=token;  
            }

            //read from file2.
            while(fgets(buffer2,SIZE256,file2)!=NULL){
                token = strtok(buffer2,"\t");
                while(token != NULL){
                    if(strcmp(token,"voluntary_ctxt_switches:")==0){
                        token = strtok(NULL,"\t");
                        printf("voluntary_ctxt_switches: %s", token); 
                    }else if(strcmp(token,"nonvoluntary_ctxt_switches:")==0){
                        token = strtok(NULL,"\t");
                        printf("nonvoluntary_ctxt_switches: %s", token);
                    }
                    else{
                        token = strtok(NULL,"\t");
                    }
                }
            }
            fclose(file2);
            fclose(file1); 
        }  
		else{
			// the other commands 
			ter = fork();
			if(ter == 0){
				if (execvp(strCmd[0], strCmd) < 0){
					printf("Error: command (%s) not found.\n", strCmd[0]);
				}
			}else if(ter > 0){
				waitpid(ter, NULL, 0);
			}else{
				printf("(%s): command not found.\n", strCmd[0]);
			}
		}
    }
    // return 0;
    
}
