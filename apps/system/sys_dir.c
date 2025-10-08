/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: the directory system server
 * handling requests to directory lookup, insertion, etc.
 */

#include "app.h"
#include <string.h>
#include <stdlib.h>

/* To understand directory management, read tools/mkfs.c */
int dir_do_lookup(int dir_ino, char* name) {
    char buf[BLOCK_SIZE];
    file_read(dir_ino, 0, buf);

    for (int i = 0, namelen = strlen(name); i < strlen(buf) - namelen; i++)
        if (!strncmp(name, buf + i, namelen) &&
            buf[i + namelen] == ' ' && (i == 0 || buf[i - 1] == ' '))
            return atoi(buf + i + namelen);

    return -1;
}

int main() {
    SUCCESS("Enter kernel process GPID_DIR");

    /* Send a notification to GPID_PROCESS */
    char buf[SYSCALL_MSG_LEN];
    strcpy(buf, "Finish GPID_DIR initialization");
    grass->sys_send(GPID_PROCESS, buf, 31);

    /* Wait for directory requests */
    while (1) {
        int sender;
        struct dir_request *req = (void*)buf;
        struct dir_reply *reply = (void*)buf;
        grass->sys_recv(&sender, buf, SYSCALL_MSG_LEN);

        switch (req->type) {
        case DIR_LOOKUP:
            reply->ino = dir_do_lookup(req->ino, req->name);
            reply->status = reply->ino == -1? DIR_ERROR : DIR_OK;
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        case DIR_INSERT:
            /* Insert directory entry */
            {
                char buf[BLOCK_SIZE];
                file_read(req->ino, 0, buf);
                
                /* Check if entry already exists */
                if (dir_do_lookup(req->ino, req->name) != -1) {
                    reply->status = DIR_ERROR;
                } else {
                    /* Append new entry: "name ino " */
                    int len = strlen(buf);
                    int name_len = strlen(req->name);
                    if (len + name_len + 10 < BLOCK_SIZE) { /* Leave room for ino and spaces */
                        sprintf(buf + len, "%s %d ", req->name, req->ino);
                        file_write(req->ino, 0, buf);
                        reply->status = DIR_OK;
                    } else {
                        reply->status = DIR_ERROR; /* Directory full */
                    }
                }
            }
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        case DIR_REMOVE:
            /* Remove directory entry */
            {
                char buf[BLOCK_SIZE];
                file_read(req->ino, 0, buf);
                
                int name_len = strlen(req->name);
                int found = 0;
                
                /* Find and remove the entry */
                for (int i = 0; i < strlen(buf) - name_len; i++) {
                    if (!strncmp(req->name, buf + i, name_len) &&
                        buf[i + name_len] == ' ' && (i == 0 || buf[i - 1] == ' ')) {
                        
                        /* Find end of entry (next space) */
                        int j = i + name_len + 1;
                        while (j < strlen(buf) && buf[j] != ' ') j++;
                        if (j < strlen(buf)) j++; /* Include the trailing space */
                        
                        /* Remove the entry by shifting remaining content */
                        int entry_len = j - i;
                        memmove(buf + i, buf + j, strlen(buf) - j + 1);
                        found = 1;
                        break;
                    }
                }
                
                if (found) {
                    file_write(req->ino, 0, buf);
                    reply->status = DIR_OK;
                } else {
                    reply->status = DIR_ERROR; /* Entry not found */
                }
            }
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        default:
            /* This part is left to students as an exercise */
            FATAL("sys_dir: request%d not implemented", req->type);
        }
    }
}
