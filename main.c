#include<stdio.h>
#include<string.h>
#define BUFFER_SIZE 1024
int main()
{
char command[BUFFER_SIZE];
printf("\n");
    printf("=========================\n");
    printf(" Welcome to Shellforge\n");
    printf("=========================\n");
    while(1)
    {
        printf("shellforge> ");
        fgets(command,
              BUFFER_SIZE,
              stdin);
        command[strcspn(command,"\n")]='\0';
        if(strcmp(command,"exit")==0)
        {
            printf("Closing Shellforge...\n");
            break;
        }
        printf("Command entered : %s\n",
                command);
    }
    return 0;
}
