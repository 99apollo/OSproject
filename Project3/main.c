#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  // time ���̺귯�� �߰�
#include <pthread.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256
#define MAX_DIRS 256
#define MAX_USERS 100

char currentPath[MAX_LINE_LENGTH];


typedef struct {
    char path[MAX_LINE_LENGTH];
    char type;
    char name[MAX_LINE_LENGTH];
    int size;
    int permission;
    char owner[MAX_LINE_LENGTH];
    char timestamp[MAX_LINE_LENGTH];
    char selfPath[MAX_LINE_LENGTH];  // ���� �ڱ� �ڽ��� ���
    int isHidden;  // ���� �÷��� �߰�
} DirectoryEntry;

typedef struct {
    char id[MAX_LINE_LENGTH];
    int uid;
    int gid;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    char path[MAX_LINE_LENGTH];
} User;

typedef struct {
    const char* line;
    const char* pattern;
    const char* filename;
    int lineCount;
    int patternLength;
    int ignoreCase;
    int invertMatch;
    int lineNumbers;
    char* lineCopy;  // ������ ���ڿ� ������
} GrepThreadData;

void concatenateWithoutSpaces(char* dest, const char* src) {
    while (*dest)
        dest++;

    while (*src) {
        if (*src != ' ') {
            *dest = *src;
            dest++;
        }
        src++;
    }

    *dest = '\0';
}

void parseDirectoryEntry(char* line, DirectoryEntry* entry) {
    sscanf(line, "%s %c %s %d %d %s %s %d %s", entry->path, &(entry->type), entry->name,
        &(entry->size), &(entry->permission), entry->owner, entry->timestamp, &(entry->isHidden), entry->selfPath);
}


void loadDirectoryEntries(const char* filename, DirectoryEntry* entries, int* numEntries) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    *numEntries = 0;
    while (fgets(line, sizeof(line), file)) {
        parseDirectoryEntry(line, &entries[*numEntries]);
        (*numEntries)++;
    }

    fclose(file);
}

void printDirectoryEntries(DirectoryEntry* entries, int numEntries, int showHidden, int showDetailed, const char* currentPath) {
    for (int i = 0; i < numEntries; i++) {
        // ������ ������ ��� showHidden�� false�� ��� ������� ����
        if (!showHidden && entries[i].isHidden)
            continue;

        // ���� ���丮 ���� ������ ��� �Ǵ� �ڱ� �Ҽ� ���丮�� ��쿡�� ���
        if (strcmp(entries[i].path, currentPath) == 0 || strcmp(entries[i].path, currentPath) == 0) {
            if (showDetailed) {
                // ���� ���� ���
                char permissionString[11];

                permissionString[0] = (entries[i].type == 'd') ? 'd' : '-';
                permissionString[1] = (entries[i].permission & 0400) ? 'r' : '-';
                permissionString[2] = (entries[i].permission & 0200) ? 'w' : '-';
                permissionString[3] = (entries[i].permission & 0100) ? 'x' : '-';
                permissionString[4] = (entries[i].permission & 040) ? 'r' : '-';
                permissionString[5] = (entries[i].permission & 020) ? 'w' : '-';
                permissionString[6] = (entries[i].permission & 010) ? 'x' : '-';
                permissionString[7] = (entries[i].permission & 04) ? 'r' : '-';
                permissionString[8] = (entries[i].permission & 02) ? 'w' : '-';
                permissionString[9] = (entries[i].permission & 01) ? 'x' : '-';
                permissionString[10] = '\0';

                printf("%s 1 ", permissionString);
                // ���� ������ ���� ���
                printf("%s ", entries[i].owner);

                // ���� ũ�� ���
                printf("%d ", entries[i].size);

                // Ÿ�ӽ����� ���
                printf("%s ", entries[i].timestamp);
            }

            // ���� �̸� ���
            printf("%s\n", entries[i].name);
        }
    }
}

void parseUser(char* line, User* user) {
    sscanf(line, "%s %d %d %d %d %d %d %d %d %s", user->id, &(user->uid), &(user->gid),
        &(user->year), &(user->month), &(user->day), &(user->hour), &(user->minute),
        &(user->second), user->path);
}

void loadUsers(const char* filename, User* users, int* numUsers) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    *numUsers = 0;
    while (fgets(line, sizeof(line), file)) {
        parseUser(line, &users[*numUsers]);
        (*numUsers)++;
    }

    fclose(file);
}

User* login(User* users, int numUsers) {
    char id[MAX_LINE_LENGTH];
    printf("Enter ID: ");
    scanf("%s", id);

    for (int i = 0; i < numUsers; i++) {
        if (strcmp(users[i].id, id) == 0) {
            printf("Login successful!\n");
            return &(users[i]);
        }
    }

    printf("Invalid ID. Login failed.\n");
    return NULL;
}



int checkReadPermission(int permission, const char* currentUser, int isOwner) {
    int ownerPermission = (permission >> 6) & 0x7; // ������ ���� ��Ʈ
    int otherPermission = (permission >> 0) & 0x7; // �׿� ����� ���� ��Ʈ

    // ���� ����ڰ� �������� ���
    if (isOwner) {
        if ((ownerPermission & 0x4) != 0)
            return 1; // �б� ����
    }
    // �׿� ������� ���
    else {
        if ((otherPermission & 0x4) != 0)
            return 1; // �б� ����
    }

    return 0; // ���� ����
}
int checkWritePermission(int permission, const char* currentUser, int isOwner) {
    int ownerPermission = (permission >> 6) & 0x7; // ������ ���� ��Ʈ
    int otherPermission = (permission >> 0) & 0x7; // �׿� ����� ���� ��Ʈ

    // ���� ����ڰ� �������� ���
    if (isOwner) {
        if ((ownerPermission & 0x2) != 0)
            return 1; // ���� ����
    }
    // �׿� ������� ���
    else {
        if ((otherPermission & 0x2) != 0)
            return 1; // ���� ����
    }

    return 0; // ���� ����
}
int checkExecutePermission(int permission, const char* currentUser, int isOwner) {
    int ownerPermission = (permission >> 6) & 0x7; // ������ ���� ��Ʈ
    int otherPermission = (permission >> 0) & 0x7; // �׿� ����� ���� ��Ʈ

    // ���� ����ڰ� �������� ���
    if (isOwner) {
        if ((ownerPermission & 0x1) != 0)
            return 1; // ���� ����
    }
    // �׿� ������� ���
    else {
        if ((otherPermission & 0x1) != 0)
            return 1; // ���� ����
    }

    return 0; // ���� ����
}



void cd(const char* directory, DirectoryEntry* entries, int numEntries, const char* currentUser) {
    char newPath[MAX_LINE_LENGTH];

    if (strcmp(directory, "/") == 0) {
        strcpy(newPath, "/");
    }
    else if (strcmp(directory, "..") == 0) {
        // Go to parent directory
        char* lastSlash = strrchr(currentPath, '/');
        if (lastSlash != NULL) {
            *lastSlash = '\0';
            if (strcmp(currentPath, "") == 0) {
                strcpy(currentPath, "/");
            }
        }
        strcpy(newPath, currentPath);
    }
    else if (directory[0] == '/') {
        // Absolute path
        strcpy(newPath, directory);
    }
    else {
        // Relative path
        strcpy(newPath, currentPath);
        if (strcmp(currentPath, "/") != 0) {
            strcat(newPath, "/");
        }
        strcat(newPath, directory);
    }
    int check = -1;
    int own = 0;
    int isValidDirectory = 0;


    if (strcmp(currentUser, "root") != 0) {
        for (int i = 0; i < numEntries; i++) {
            if (strcmp(entries[i].selfPath, newPath) == 0 && entries[i].type == 'd') {
                if (strcmp(currentUser, entries[i].owner) == 0) {
                    check = checkReadPermission(entries[i].permission, currentUser, 1);
                    printf("owner\n");
                }
                else {
                    check = checkReadPermission(entries[i].permission, currentUser, 0);
                }
            }
        }
        if (check == 0) {
            printf("no Permission\n");
            return;
        }
    }


    for (int i = 0; i < numEntries; i++) {
        if (strcmp(entries[i].path, newPath) == 0 && entries[i].type == 'd') {
            isValidDirectory = 1;

            strcpy(currentPath, newPath);  // Update currentPath
            break;
        }
    }

    if (!isValidDirectory) {
        isValidDirectory = 0; // Reset isValidDirectory flag

        // Check if the directory exists in the selfPath of entries
        for (int i = 0; i < numEntries; i++) {
            if (strcmp(entries[i].selfPath, newPath) == 0 && entries[i].type == 'd') {
                isValidDirectory = 1;
                strcpy(currentPath, newPath);  // Update currentPath
                break;
            }
        }
    }

    if (!isValidDirectory) {
        printf("Invalid directory path: %s\n", newPath);
    }
}

void ls(DirectoryEntry* entries, int numEntries, const char* currentPath, int showHidden, int showDetailed, const char* currentUser) {

    int check = -1;
    if (strcmp(currentUser, "root") != 0) {
        if (strcmp(currentPath, "/") != 0) {
            for (int i = 0; i < numEntries; i++) {
                if (strcmp(entries[i].selfPath, currentPath) == 0 && entries[i].type == 'd') {
                    if (strcmp(currentUser, entries[i].owner) != 0) {
                        check = checkReadPermission(entries[i].permission, currentUser, 1);
                    }
                    else {
                        check = checkReadPermission(entries[i].permission, currentUser, 0);
                    }
                }
            }
            if (check == 0) {
                printf("no Permission\n");
                return;
            }
        }
    }
    printf("Directory listing for %s:\n", currentPath);
    printDirectoryEntries(entries, numEntries, showHidden, showDetailed, currentPath);
}


void updateSystemFile(const char* filename, DirectoryEntry* entries, int numEntries) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        exit(1);
    }

    for (int i = 0; i < numEntries; i++) {
        fprintf(file, "%s %c %s %d %d %s %s %d %s\n", entries[i].path, entries[i].type, entries[i].name,
            entries[i].size, entries[i].permission, entries[i].owner, entries[i].timestamp, entries[i].isHidden, entries[i].selfPath);
    }

    fclose(file);
}

void chown(const char* filename, const char* owner, DirectoryEntry* entries, int numEntries, const char* currentPath, User* users, int numUsers, const char* currentUser) {
    int found = 0;
    for (int i = 0; i < numEntries; i++) {
        if (strcmp(entries[i].name, filename) == 0 && strcmp(entries[i].path, currentPath) == 0) {
            if (entries[i].type == 'f' || entries[i].type == 'd') {
                // Check if the owner exists in the User.txt file
                int isValidOwner = 0;
                for (int j = 0; j < numUsers; j++) {
                    if (strcmp(users[j].id, owner) == 0) {
                        isValidOwner = 1;
                        break;
                    }
                }
                int check = -1;
                if (strcmp(currentUser, "root") != 0) {
                    if (entries[i].owner == currentUser) {
                        check = checkExecutePermission(entries[i].permission, currentUser, 1);
                    }
                    else {
                        check = checkExecutePermission(entries[i].permission, currentUser, 0);
                    }
                    if (check == 0) {
                        printf("no Permission\n");
                        return;
                    }
                }
                if (isValidOwner) {
                    // Change ownership of file or directory
                    strcpy(entries[i].owner, owner);
                    found = 1;
                }
                else {
                    printf("User '%s' does not exist. Ownership not changed.\n", owner);
                }

                break;
            }
        }
    }

    if (found) {
        printf("Ownership of '%s' changed to '%s'.\n", filename, owner);
        updateSystemFile("system.txt", entries, numEntries);
    }
    else {
        printf("File or directory '%s' not found in the current directory.\n", filename);
    }
}


void createDirectory(const char* name, const char* path, const char* currentUser, DirectoryEntry* entries, int* numEntries) {
    // ���丮 ���� ���� �� �ʱ�ȭ
    printf("CD: %s %s %s \n", name, path, currentUser);
    DirectoryEntry newDir;
    strcpy(newDir.path, path);
    newDir.type = 'd';  // ���丮 Ÿ��
    strcpy(newDir.name, name);
    newDir.size = 4096;  // ���丮 ũ�� (����)
    newDir.permission = 0755;  // �۹̼� (755)
    strcpy(newDir.owner, currentUser);  // ���� ����� ���̵�� ����
    // ���� �ð��� Ÿ�ӽ������� ����
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(newDir.timestamp, sizeof(newDir.timestamp), "%Y-%m-%d", timeinfo);
    if (strcmp(path, "/") == 0) {
        strcpy(newDir.selfPath, "/");
        strcat(newDir.selfPath, name);
    }
    else {
        strcpy(newDir.selfPath, path);
        strcat(newDir.selfPath, "/");
        strcat(newDir.selfPath, name);
    }
    newDir.isHidden = 0;  // ���� �÷���
    printf("%s %s %s %d\n", newDir.path, newDir.name, newDir.selfPath, *numEntries);
    // ���丮 ������ system.txt�� �߰�
    entries[*numEntries] = newDir;
    (*numEntries)++;
    // system.txt�� ����� ���丮 ���� ���� (�߰��� ���丮 ����)
    updateSystemFile("system.txt", entries, *numEntries);

    printf("Directory '%s' created.\n", newDir.name);
}



void mkdirWithOption(const char* name, const char* path, char* currentUser, DirectoryEntry* entries, int* numEntries, int isRecursive) {
    // -p �ɼ� �ִ� ���
    if (isRecursive) {
        // ���� ������� Ȯ��
        int isAbsolutePath = (path[0] == '/');

        // �ߺ� Ȯ���� ������ ��� ���丮��
        const char* lastDir = strrchr(path, '/');
        if (lastDir != NULL) {
            lastDir++; // '/' ���� ���İ� ���丮 �̸�

            // �ߺ� Ȯ��
            int isDirExists = 0;
            for (int i = 0; i < *numEntries; i++) {
                if (entries[i].type == 'd' && strcmp(entries[i].path, path) == 0 && strcmp(entries[i].name, lastDir) == 0) {
                    printf("Directory '%s' already exists.\n", lastDir);
                    isDirExists = 1;
                    break; // �ߺ��� ���丮 �̸��� ������ �ݺ��� ����
                }
            }

            if (!isDirExists) {
                // �߰� ��� ���丮 ����
                char currentPathCopy[MAX_LINE_LENGTH];
                strcpy(currentPathCopy, isAbsolutePath ? "/" : currentPath); // ���� ����� ��� '/'�� �����ϰ�, ��� ����� ��� currentPath�� �����մϴ�.

                char* pathCopy = strdup(path);
                char* token = strtok(pathCopy, "/");
                char tokenName[MAX_LINE_LENGTH]; // �� ����� ���丮 �̸��� �����ϴ� ����
                int item = 0;
                char tokenName2[MAX_LINE_LENGTH];
                char lastTokenName[MAX_LINE_LENGTH]; // ������ ����� ���丮 �̸��� �����ϴ� ����
                while (token != NULL) {
                    strcpy(tokenName, token);
                    if (item != 0) {
                        strcat(currentPathCopy, tokenName2);
                    }
                    strcpy(tokenName2, token);


                    // �ߺ� Ȯ��
                    isDirExists = 0;
                    for (int i = 0; i < *numEntries; i++) {
                        if (entries[i].type == 'd' && strcmp(entries[i].path, currentPathCopy) == 0 && strcmp(entries[i].name, tokenName) == 0) {
                            isDirExists = 1;
                            int check = -1;
                            if (strcmp(currentUser, "root") != 0) {
                                if (strcmp(currentUser, entries[i].owner) == 0) {
                                    check = checkExecutePermission(entries[i].permission, currentUser, 1);
                                }
                                else {
                                    check = checkExecutePermission(entries[i].permission, currentUser, 0);
                                }
                                if (check == 0) {
                                    printf("no Permission : Can't Execute mkdir \n");
                                    return;
                                }
                            }
                            break; // �ߺ��� ���丮 �̸��� ������ �ݺ��� ����
                        }
                    }

                    // ���丮�� �������� ������ ����
                    if (!isDirExists) {
                        createDirectory(tokenName, currentPathCopy, currentUser, entries, numEntries);
                    }
                    if (item == 0 && strcmp("/", currentPathCopy) != 0) {
                        strcat(currentPathCopy, isAbsolutePath ? "" : "/");
                    }
                    if (item != 0) {
                        strcat(currentPathCopy, "/");
                    }
                    strcpy(lastTokenName, token); // ������ ���丮 �̸� ����
                    token = strtok(NULL, "/");
                    item++;
                }
                // ������ ���丮�� ��츦 ó��
                if (lastTokenName[0] != '\0') {
                    strcat(currentPathCopy, lastTokenName);

                    // �ߺ� Ȯ��
                    isDirExists = 0;
                    for (int i = 0; i < *numEntries; i++) {
                        if (entries[i].type == 'd' && strcmp(entries[i].path, currentPathCopy) == 0 && strcmp(entries[i].name, name) == 0) {
                            isDirExists = 1;
                            printf("Already exists\n");
                            return; // �ߺ��� ���丮 �̸��� ������ �ݺ��� ����
                        }
                    }

                    // ���丮�� �������� ������ ����
                    if (!isDirExists) {
                        createDirectory(name, currentPathCopy, currentUser, entries, numEntries);
                    }
                }
                item = 0;
                free(pathCopy);
            }
        }
    }
    else {
        // ���� ������� Ȯ��
        int isAbsolutePath = (path[0] == '/');
        // �ߺ� Ȯ��
        int isDirExists = 0;
        for (int i = 0; i < *numEntries; i++) {
            if (entries[i].type == 'd' && strcmp(entries[i].path, path) == 0 && strcmp(entries[i].name, name) == 0) {
                printf("Directory '%s' already exists.\n", name);
                return; // �ߺ��� ���丮 �̸��� ������ �Լ� ����
            }
        }
        // �߰� ��� ���丮 ����
        char currentPathCopy[MAX_LINE_LENGTH];
        strcpy(currentPathCopy, path);

        if (isAbsolutePath) {
            // ���� ����� ���
            char* currentPathCopy = strdup(path);
            printf("rererer\n");
            char* token = strtok(currentPathCopy, "/");
            char tokenName[MAX_LINE_LENGTH]; // �� ����� ���丮 �̸��� �����ϴ� ����

            while (token != NULL) {
                strcpy(tokenName, token);
                printf("\n%s %s %s %s\n", path, currentPathCopy, tokenName, token);
                // �ߺ� Ȯ��
                isDirExists = 0;
                int check = -1;
                int item = -1;
                for (int i = 0; i < *numEntries; i++) {
                    printf("\n :%s path:%s, %s TokenName:%s , %s  type: %c\n", path, currentPathCopy, entries[i].selfPath, tokenName, entries[i].name, entries[i].type);
                    if (entries[i].type == 'd' && strcmp(entries[i].selfPath, currentPathCopy) == 0 && strcmp(entries[i].name, tokenName) == 0) {
                        item = i;
                        isDirExists = 1;
                        // ������ ��� ���ϱ�
                        char remainingPath[MAX_LINE_LENGTH];
                        strcpy(remainingPath, path + strlen(currentPathCopy)); // ���� ��θ� ������ ������ ��� ����
                        printf("rere %s\n", remainingPath);
                        // �߰� ��η� currentPathCopy�� �߰�
                        strcat(currentPathCopy, remainingPath);
                        break; // �ߺ��� ���丮 �̸��� ������ �ݺ��� ����
                    }
                }
                printf("%d, %d\n", item, isDirExists);
                if (strcmp(currentUser, "root") != 0 && item != -1) {
                    if (strcmp(currentUser, entries[item].owner) == 0) {
                        check = checkExecutePermission(entries[item].permission, currentUser, 1);
                        printf("owner\n");
                    }
                    else {
                        check = checkExecutePermission(entries[item].permission, currentUser, 0);
                    }
                    if (check == 0) {
                        printf("no Permission : Can't Execute mkdir \n");
                        free(currentPathCopy); // �޸� ����
                        return;
                    }
                }
                // ���丮�� �������� ������ ����
                if (!isDirExists) {
                    printf("Path is not correct: Directory '%s' does not exist.\n", tokenName);
                    free(currentPathCopy); // �޸� ����
                    return;
                }

                token = strtok(NULL, "/");
                printf("\n123 %s %s %s\n", path, currentPathCopy, tokenName);
                item = -1;
            }

            // ���丮 ���� ���� �߰�
        }




        else {
            printf("test2\n");
            // ��� ����� ���
            char* currentDir = currentPath;

            if (strcmp(currentDir, "/") != 0)
                strcat(currentDir, "/");
            strcat(currentDir, path);

            strcpy(currentPathCopy, currentDir);
        }
        printf("test\n");
        // ������ ���丮�� ��츦 ó��
        strcat(currentPathCopy, "/");
        strcat(currentPathCopy, name);
        // ���丮�� �������� ������ ����
        createDirectory(name, path, currentUser, entries, numEntries);

        return;

    }


}

int checkUserPermission(const char* filename, const char* currentPath, User* currentUser, DirectoryEntry* entries, int numEntries) {
    for (int i = 0; i < numEntries; i++) {
        if (strcmp(entries[i].name, filename) == 0 && strcmp(entries[i].path, currentPath) == 0) {
            if (strcmp(entries[i].owner, currentUser->id) == 0 || strcmp(currentUser->id, "root") == 0) {
                return 1;  // Permission granted
            }
            else {
                return 0;  // Permission denied
            }
        }
    }
    return -1;  // File not found
}

void chmod_file(const char* filename, int permission, DirectoryEntry* entries, int numEntries, const char* currentPath, User* currentUser) {
    int permissionResult = checkUserPermission(filename, currentPath, currentUser, entries, numEntries);

    if (permissionResult == 1) {
        // Permission granted
        for (int i = 0; i < numEntries; i++) {
            if (strcmp(entries[i].name, filename) == 0 && strcmp(entries[i].path, currentPath) == 0) {
                entries[i].permission = permission;
                break;
            }
        }
        printf("Permission of '%s' changed to %o.\n", filename, permission);
        updateSystemFile("system.txt", entries, numEntries);
    }
    else if (permissionResult == 0) {
        // Permission denied
        printf("Permission denied. You do not have sufficient privileges to modify the file '%s'.\n", filename);
    }
    else {
        // File not found
        printf("File '%s' not found in the current directory.\n", filename);
    }
}

void cat(const char* currentPath, const char* filename, int numEntries, DirectoryEntry* entries, const char* currentUser) {
    char filepath[256];
    sprintf(filepath, "%s/%s", currentPath, filename);
    int found = 0;
    int check = -1;
    for (int i = 0; i < numEntries; i++) {
        if (entries[i].type == 'f' && strcmp(entries[i].name, filename) == 0 && strcmp(entries[i].path, currentPath) == 0) {
            if (strcmp(currentUser, "root") != 0) {
                if (strcmp(currentUser, entries[i].owner) != 0) {
                    check = checkReadPermission(entries[i].permission, currentUser, 1);
                }
                else {
                    check = checkReadPermission(entries[i].permission, currentUser, 0);
                }
                if (check == 0) {
                    printf("no Permission : Can't Execute cat %s \n", filename);
                    return;
                }
            }

            FILE* file = fopen(filename, "r");
            if (file != NULL) {
                char ch;
                int isLineEmpty = 1; // ���� ����ִ��� ���θ� Ȯ���ϱ� ���� ����

                while ((ch = fgetc(file)) != EOF) {
                    if (ch != '\n' && ch != '\r') {
                        isLineEmpty = 0;
                        printf("%c", ch);
                    }
                    else if (!isLineEmpty) {
                        printf("\n");
                        isLineEmpty = 1;
                    }
                }

                fclose(file);
                found = 1;
                break;
            }
        }

    }
    if (found == 0)
    {
        printf("Can not find File.");
    }
    printf("\n");
}

void catNumbered(const char* currentPath, const char* filename, int numEntries, DirectoryEntry* entries, char* currentUser) {
    char filepath[256];
    sprintf(filepath, "%s/%s", currentPath, filename);
    int found = 0;
    int lineNumber = 1;
    int check = -1;
    int shouldPrint = 0; // �ٿ� ������ �ִ� ��� ��� ���θ� �����ϱ� ���� ����
    for (int i = 0; i < numEntries; i++) {
        if (entries[i].type == 'f' && strcmp(entries[i].name, filename) == 0 && strcmp(entries[i].path, currentPath) == 0) {
            if (strcmp(currentUser, "root") != 0) {
                if (entries[i].owner == currentUser) {
                    check = checkReadPermission(entries[i].permission, currentUser, 1);
                }
                else {
                    check = checkReadPermission(entries[i].permission, currentUser, 0);
                }
                if (check == 0) {
                    printf("no Permission : Can't Execute cat %s \n", filename);
                    return;
                }
            }
            FILE* file = fopen(filename, "r");
            if (file != NULL) {
                char line[256];
                while (fgets(line, sizeof(line), file) != NULL) {
                    if (line[0] != '\n') { // �ٿ� ������ �ִ� ���
                        printf("%d\t %s", lineNumber, line);
                        shouldPrint = 1;
                    }
                    else { // �� ���� ���
                        printf("%d\n", lineNumber);
                    }
                    lineNumber++;
                }
                fclose(file);
                found = 1;
            }
        }

    }
    if (found == 0)
    {
        printf("Can not find File.");
    }
    printf("\n");

}

//void grep(const char* currentPath, const char* pattern, const char* filename, int numEntries, DirectoryEntry* entries, const char* currentUser, int ignoreCase, int invertMatch, int lineNumbers) {
//    char filePath[MAX_LINE_LENGTH];
//    if (strcmp(currentPath, "/") == 0) {
//        strcpy(filePath, currentPath);
//        strcat(filePath, filename);
//    }
//    else {
//        strcpy(filePath, currentPath);
//        strcat(filePath, "/");
//        strcat(filePath, filename);
//    }
//    int location = 0;
//    for (int i = 0; i < numEntries; i++) {
//        if (entries[i].type == 'f' && strcmp(entries[i].selfPath,filePath) == 0 && strcmp(entries[i].name, filename) == 0) {
//            location = 1;
//        }
//    }
//    if (location == 0){
//        printf("Can't find file!\n");
//        return;
//    }
//    FILE* file = fopen(filename, "r");
//    if (file == NULL) {
//        printf("Failed to open file: %s\n", filename);
//        return;
//    }
//    else {
//    }
//    char line[MAX_LINE_LENGTH];
//    int lineCount = 0;
//    int patternLength = strlen(pattern);
//
//    while (fgets(line, sizeof(line), file) != NULL) {
//        lineCount++;
//        int lineLength = strlen(line);
//
//        // Remove newline character from line
//        if (line[lineLength - 1] == '\n') {
//            line[lineLength - 1] = '\0';
//            lineLength--;
//        }
//
//        // Ignore case if specified
//        if (ignoreCase) {
//            char* lowerLine = strdup(line);
//            for (int i = 0; i < lineLength; i++) {
//                lowerLine[i] = tolower(lowerLine[i]);
//            }
//
//            if (invertMatch) {
//                if (strstr(lowerLine, pattern) == NULL) {
//                    if (lineNumbers) {
//                        printf("%s:%d: %s\n", filename, lineCount, line);
//                    }
//                    else {
//                        printf("%s\n", line);
//                    }
//                }
//            }
//            else {
//                if (strstr(lowerLine, pattern) != NULL) {
//                    if (lineNumbers) {
//                        printf("%s:%d: %s\n", filename, lineCount, line);
//                    }
//                    else {
//                        printf("%s\n", line);
//                    }
//                }
//            }
//
//            free(lowerLine);
//        }
//        else {
//            if (invertMatch) {
//                if (strstr(line, pattern) == NULL) {
//                    if (lineNumbers) {
//                        printf("%s:%d: %s\n", filename, lineCount, line);
//                    }
//                    else {
//                        printf("%s\n", line);
//                    }
//                }
//            }
//            else {
//                if (strstr(line, pattern) != NULL) {
//                    if (lineNumbers) {
//                        printf("%s:%d: %s\n", filename, lineCount, line);
//                    }
//                    else {
//                        printf("%s\n", line);
//                    }
//                }
//            }
//        }
//    }
//
//    fclose(file);
//}

void* grepThread(void* arg) {
    GrepThreadData* data = (GrepThreadData*)arg;
    char* line = data->lineCopy;  // Duplicate line to writable memory
    const char* pattern = data->pattern;
    const char* filename = data->filename;
    int lineCount = data->lineCount;
    int patternLength = data->patternLength;
    int ignoreCase = data->ignoreCase;
    int invertMatch = data->invertMatch;
    int lineNumbers = data->lineNumbers;

    int lineLength = strlen(line);
    
    // Remove newline character from line
    if (line[lineLength - 1] == '\n') {
        line[lineLength - 1] = '\0';
        lineLength--;
    }

    // Convert line to lowercase if ignoreCase is true
    if (ignoreCase) {
        char* lowerLine = strdup(line);
        for (int i = 0; i < lineLength; i++) {
            lowerLine[i] = tolower(lowerLine[i]);
        }
        line = lowerLine;
    }

    // Check for match based on options
    if (ignoreCase == 0 && invertMatch == 0 && lineNumbers == 0) {
        if (strstr(line, pattern) != NULL) {
            printf("%s\n", line);
        }
    }
    else if (ignoreCase == 0 && invertMatch == 0 && lineNumbers == 1) {

        if (strstr(line,pattern) != NULL) {
            printf("%d:%s: %s\n", lineCount, filename, line);
        }
    }
    else if(ignoreCase == 0 && invertMatch == 1 && lineNumbers == 0)
    {
        if (strstr(line,pattern) == NULL) {
            printf("%s\n", line);
        }
    }
    else if (ignoreCase == 1 && invertMatch == 0 && lineNumbers == 0) 
    {
        printf("100 : %d", strcasestr(line, pattern));
        if (strcasestr(line, pattern) != NULL) {
            printf("%s\n", line);
        }
    }


    free(line);  // Free allocated memory

    return NULL;
}

void grep(const char* currentPath, const char* pattern, const char* filename, int numEntries, DirectoryEntry* entries, const char* currentUser, int ignoreCase, int invertMatch, int lineNumbers) {

    char filePath[MAX_LINE_LENGTH];
    if (strcmp(currentPath, "/") == 0) {
        strcpy(filePath, currentPath);
        strcat(filePath, filename);
    }
    else {
        strcpy(filePath, currentPath);
        strcat(filePath, "/");
        strcat(filePath, filename);
    }
    int location = 0;
    int check = -1;
    for (int i = 0; i < numEntries; i++) {
        if (entries[i].type == 'f' && strcmp(entries[i].selfPath, filePath) == 0 && strcmp(entries[i].name, filename) == 0) {
            if (strcmp(currentUser, "root") != 0) {
                if (entries[i].owner == currentUser) {
                    check = checkReadPermission(entries[i].permission, currentUser, 1);
                }
                else {
                    check = checkReadPermission(entries[i].permission, currentUser, 0);
                }
                if (check == 0) {
                    printf("no Permission : Can't Read grep %s \n", filename);
                    return;
                }
            }

            location = 1;
        }
    }
    if (location == 0) {
        printf("Can't find file!\n");
        return;
    }
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        return;
    }
    char line[MAX_LINE_LENGTH];
    int lineCount = 0;
    int patternLength = strlen(pattern);

    // Create an array of thread IDs
    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * MAX_LINE_LENGTH);
    int threadCount = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        lineCount++;

        GrepThreadData* data = (GrepThreadData*)malloc(sizeof(GrepThreadData));
        data->line = line;
        data->lineCopy = strdup(line);  // line�� �����Ͽ� lineCopy�� ����
        data->pattern = pattern;
        data->filename = filename;
        data->lineCount = lineCount;
        data->patternLength = patternLength;
        data->ignoreCase = ignoreCase;
        data->invertMatch = invertMatch;
        data->lineNumbers = lineNumbers;

        // Create a thread to process each line
        pthread_create(&threads[threadCount], NULL, grepThread, (void*)data);
        threadCount++;
    }

    // Wait for all threads to finish
    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], NULL);
    }

    fclose(file);
    free(threads);
}

int main() {
    DirectoryEntry entries[MAX_DIRS];
    int numEntries = 0;
    loadDirectoryEntries("system.txt", entries, &numEntries);

    User users[MAX_USERS];
    int numUsers = 0;
    loadUsers("User.txt", users, &numUsers);

    User* currentUser = login(users, numUsers);
    if (currentUser == NULL) {
        return 0;
    }

    strcpy(currentPath, "/");
    char command[MAX_LINE_LENGTH];
    char buffer[MAX_LINE_LENGTH];
    fgets(buffer, sizeof(buffer), stdin);  // Read and discard the initial input

    while (1) {
        printf("%s@osproject : %s> ", currentUser->id, currentPath);
        fgets(command, sizeof(command), stdin);
        command[strlen(command) - 1] = '\0';  // Remove trailing newline character

        if (strcmp(command, "exit") == 0) {
            break;
        }
        else if (strncmp(command, "cd ", 3) == 0) {
            char directory[MAX_LINE_LENGTH];
            sscanf(command, "cd %s", directory);
            cd(directory, entries, numEntries, currentUser->id);
        }
        else if (strcmp(command, "ls") == 0) {
            ls(entries, numEntries, currentPath, 0, 0, currentUser->id);
        }
        else if (strcmp(command, "ll") == 0) {
            ls(entries, numEntries, currentPath, 0, 1, currentUser->id);
        }
        else if (strcmp(command, "la") == 0) {
            ls(entries, numEntries, currentPath, 1, 0, currentUser->id);
        }
        else if (strncmp(command, "chown ", 6) == 0) {
            char filename[MAX_LINE_LENGTH];
            char owner[MAX_LINE_LENGTH];
            sscanf(command, "chown %s %s", filename, owner);
            chown(filename, owner, entries, numEntries, currentPath, users, numUsers, currentUser->id);
        }
        else if (strncmp(command, "mkdir ", 6) == 0) {
            char name[MAX_LINE_LENGTH];
            if (strstr(command, "-p") != NULL) {
                sscanf(command, "mkdir -p %s", name);
                // -p �ɼ��� �ִ� ���
                char* pathStart = strchr(command, ' ');
                if (pathStart != NULL) {
                    pathStart += strspn(pathStart, " \t"); // ���� �� �� ���ڸ� �ǳʶݴϴ�.
                    if (strncmp(pathStart, "-p ", 3) == 0) {
                        char* path = pathStart + 3; // '-p ' ������ ��θ� ����ŵ�ϴ�.

                        // ���丮 �̸� ����
                        const char* lastDir = strrchr(path, '/');
                        if (lastDir != NULL) {
                            lastDir++; // '/' ���� ���İ� ���丮 �̸�
                            strcpy(name, lastDir);
                        }
                        // ��� ����
                        size_t pathLength = lastDir - path;
                        char extractedPath[MAX_LINE_LENGTH];
                        strncpy(extractedPath, path, pathLength);
                        extractedPath[pathLength] = '\0';
                        mkdirWithOption(name, extractedPath, currentUser->id, entries, &numEntries, 1);
                    }
                }
                else {
                    printf("Invalid command format.\n");
                }
            }
            else {
                // -p �ɼ��� ���� ���
                char* pathStart = strchr(command, ' ');
                char item[MAX_LINE_LENGTH] = "";
                if (pathStart != NULL) {
                    pathStart += strspn(pathStart, " \t"); // ���� �� �� ���ڸ� �ǳʶݴϴ�.
                    // ��� ����
                    char* pathEnd = strchr(pathStart, ' ');
                    if (pathStart[0] != '/') {
                        if (strcmp(currentPath, "/") == 0) {
                            strcat(item, currentPath);
                        }
                        else {
                            strcat(item, currentPath);
                            strcat(item, "/");
                        }

                    }

                    strcat(item, pathStart);
                    const char* lastDir = strrchr(item, '/');

                    if (lastDir != NULL) {
                        lastDir++; // '/' ���� ���İ� ���丮 �̸�
                        strcpy(name, lastDir);
                    }
                    size_t itemLength = strlen(item);
                    for (int i = itemLength - 1; i >= 0; i--) {
                        if (item[i] == '/') {
                            if (i == 0) {
                                item[i + 1] = '\0';
                                break;
                            }
                            item[i] = '\0';
                            break;
                        }
                    }

                    mkdirWithOption(name, item, currentUser->id, entries, &numEntries, 0);
                }


            }
        }
        else if (strncmp(command, "chmod ", 6) == 0) {
            char permissionStr[4];
            char filename[MAX_LINE_LENGTH];
            sscanf(command, "chmod %3s %s", permissionStr, filename);
            int permission = strtol(permissionStr, NULL, 8);
            chmod_file(filename, permission, entries, numEntries, currentPath, currentUser);
        }

        else if (strncmp(command, "cat ", 4) == 0) {
            char filename[MAX_LINE_LENGTH];
            sscanf(command, "cat %s", filename);

            // cat ��ɾ� ����
            if (strlen(command) > 4 && command[4] == '-') {
                // cat -n ����
                sscanf(command, "cat -n %s", filename);
                catNumbered(currentPath, filename, numEntries, entries, currentUser->id);
            }
            else {
                // cat ����
                cat(currentPath, filename, numEntries, entries, currentUser->id);
            }
        }
        else if (strncmp(command, "grep ", 5) == 0) {
            char options[MAX_LINE_LENGTH];
            char pattern[MAX_LINE_LENGTH];
            char filename[MAX_LINE_LENGTH];
            sscanf(command, "grep %[^\n]s", options);

            // Initialize options
            int ignoreCase = 0;
            int invertMatch = 0;
            int lineNumbers = 0;

            // Parse tokens
            char* token = strtok(options, " ");
            int optionCount = 0;
            while (token != NULL) {
                if (token[0] == '-') {
                    // Process options
                    for (int i = 1; i < strlen(token); i++) {
                        switch (token[i]) {
                        case 'i':
                            ignoreCase = 1;
                            break;
                        case 'v':
                            invertMatch = 1;
                            break;
                        case 'n':
                            lineNumbers = 1;
                            break;
                            // Add more options if needed
                        default:
                            // Invalid option
                            break;
                        }
                    }
                }
                else {
                    // Process pattern and filename
                    if (optionCount == 0) {
                        strncpy(pattern, token, MAX_LINE_LENGTH);
                    }
                    else if (optionCount == 1) {
                        strncpy(filename, token, MAX_LINE_LENGTH);
                    }
                    optionCount++;
                }
                token = strtok(NULL, " ");
            }

            grep(currentPath, pattern, filename, numEntries, entries, currentUser->id, ignoreCase, invertMatch, lineNumbers);
        }else {
            printf("Invalid command.\n");
        }
    }

    return 0;
}