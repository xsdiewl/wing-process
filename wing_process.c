/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | php win32 deamon process support
  | Author: yuyi 
  | Email: 297341015@qq.com
  | Home: http://www.itdfy.com/
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_wing_process.h"

#ifdef PHP_WIN32
#include "helper/wing_ntdll.h"
#include "Shlwapi.h"
#pragma comment(lib,"Shlwapi.lib")
#include "Psapi.h"
#pragma comment(lib,"Psapi.lib")
#else
typedef int BOOL;
#define INFINITE 0
#define MAX_PATH 256
#include <sys/types.h>   // 提供类型 pid_t 的定义
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

/**
 * linux或者mac查找命令所在路径，使用完需要free释放资源
 * 如：getCommandPath("php"); //返回 /usr/bin/php
 */
char* getCommandPath(const char* command) {

    char *env = getenv("PATH");
    unsigned long start = (unsigned long)env;
    size_t len = strlen(env);
    unsigned long pos = (unsigned long)env;
    unsigned long size = 0;
    char *temp = NULL;
    unsigned long command_len = strlen(command)+1;
    
    while(1) {
        char t = ((char*)start)[0];
        
        if (t == ':' ) {
            size = start - pos;
            temp = (char *)malloc(size+command_len+1);
            memset(temp, 0, size+command_len+1);
            strncpy(temp, (char*)pos, size);
            char *base = (char*)((unsigned long)temp + strlen(temp));
            strcpy(base, "/");
            strcpy((char*)((unsigned long)base + 1), command);
            
            //std::cout << temp << "\r\n";
            if (access(temp, F_OK) == 0) {
                //std::cout << command << " path is : " << temp << "\r\n";
                return temp;
            }
            
            pos = start+1;
            free(temp);
            temp = NULL;
        }
        
        if (start >= ((unsigned long)env+len) ) {
            break;
        }
        
        start++;
    }
    
    
    
    size = (unsigned long)env+len - pos;
    
    temp = (char *)malloc(size+command_len+1);
    memset(temp, 0, size+command_len+1);
    strncpy(temp, (char*)pos, size);
   
    char *base = (char*)((unsigned long)temp + strlen(temp));
    strcpy(base, "/");
    strcpy((char*)((unsigned long)base + 1), command);
    
    //std::cout << temp << "\r\n";
    if (access(temp, F_OK) == 0) {
        //std::cout << command << " path is : " << temp << "\r\n";
        return temp;
    }
    free(temp);
    temp = NULL;
    return NULL;
}


#endif

#define WING_ERROR_FAILED  0
#define WING_ERROR_SUCCESS 1


BOOL wing_check_is_runable(const char *file);
BOOL file_is_php(const char *file)
{
    FILE *handle = fopen(file, "r");
    char *line1 = (char*)malloc(8);
    memset(line1, 0 , 7);
    fgets(line1, 7, handle);
    //std::cout << line1 << "\r\n";
    char *find = strstr(line1, "<?php");
    if(find == line1 ) {
        free(line1);
        fclose(handle);
        return 1;//std::cout << "line1是php文件\r\n";
    }

    char *line2 = (char*)malloc(8);
    memset(line2, 0 , 7);
    fgets(line2, 7, handle);
    //std::cout << line2 << "\r\n";
    char *find2 =strstr(line2, "<?php");
    if(find2 == line2 ) {
        free(line2);
        fclose(handle);
        //std::cout << "line2是php文件\r\n";
        return 1;
    }
    fclose(handle);
    free(line1);
    free(line2);
    return 0;
}

static int le_wing_process;
char *PHP_PATH = NULL;

zend_class_entry *wing_process_ce;

/**
 * ����ļ��Ƿ�Ϊ��ִ���ļ�
 * 
 * @param string �ļ��������Դ�·����Ҳ���Բ���·����
 * @return bool
 */
BOOL wing_check_is_runable(const char *file) {

	char *begin = NULL;
	char *find = NULL;

	if (file[0] == '\'' || file[0] == '\"') {
		begin = (char*)(file + 1);
	}
	else {
		begin = (char*)file;
	}


	find = strchr(begin, '.');
	if (!find)
	{
		return 0;
	}
	const char *p = strchr(begin, '.') + 1;

	char *ext = (char*)emalloc(4);
	memset(ext, 0, 4);

	#ifdef PHP_WIN32
	strncpy_s(ext, 4, p, 3);
	#else
	strncpy(ext, p, 3);
	#endif

	BOOL is_run = 0;
	if (strcmp(ext, "exe") == 0 || strcmp(ext, "bat") == 0)
	{
		is_run = 1;
	}
	efree(ext);
	return is_run;
}

/**
 * ���캯��
 * @param $file
 * @param $ouput
 */
ZEND_METHOD(wing_process, __construct) 
{

	char *file        = NULL;  
	char *output_file = NULL;
	int file_len      = 0;
	int output_len    = 0;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
		"s|s", &file, &file_len, &output_file, &output_len)) {
		return;
	}

	zend_update_property_string( wing_process_ce, getThis(),
		"file", strlen("file"), file TSRMLS_CC );
	if (output_file) {
	    zend_update_property_string( wing_process_ce, getThis(),
		    "output_file", strlen("output_file"), output_file TSRMLS_CC);
	}

	int size = strlen(PHP_PATH) + file_len + 2;
	char *command_line = NULL;

	//������������� ����Ϊ��Ҫͨ�����н���id��������
	if (is_numeric_string(file, strlen(file), NULL, NULL, 0)) {
	    #ifdef PHP_WIN32
		PROCESSINFO *item = new PROCESSINFO();
		DWORD process_id  = zend_atoi(file, strlen(file));
		WingQueryProcessByProcessID(item, process_id);
		if (item){
			spprintf(&command_line, size, "%s", item->command_line);
			zend_update_property_long(wing_process_ce, getThis(), "process_id", strlen("process_id"), process_id TSRMLS_CC);
			zend_update_property_long(wing_process_ce, getThis(), "thread_id", strlen("thread_id"), 0 TSRMLS_CC);
			delete item;
		}
		#endif
	} else {
	    #ifdef PHP_WIN32
		if (!wing_check_is_runable((const char*)file)){
			spprintf(&command_line,size,"%s %s\0", PHP_PATH, file);
		} else {
			spprintf(&command_line, size, "%s\0", file);
		}
		#else
		spprintf(&command_line, size, "%s", file);
		#endif
	}

	zend_update_property_string(wing_process_ce, getThis(), "command_line", strlen("command_line"), command_line TSRMLS_CC);
	if (command_line) {
	    efree(command_line);
	}
}

/***
 * ��������
 */
ZEND_METHOD(wing_process, __destruct) {
	 
	zval *_pi = zend_read_property(wing_process_ce, getThis(),
		"process_info_pointer", strlen("process_info_pointer"), 0, 0 TSRMLS_CC);

    #ifdef PHP_WIN32
	PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *)Z_LVAL_P(_pi);

	if (pi) {
		CloseHandle(pi->hProcess);
		CloseHandle(pi->hThread);
		delete pi;
	}
	#endif
}

/**
 * ��ʼִ��
 *
 * @param int �Ƿ��ض���������������Ϊ������ض��������Ϊ�ػ����̣�Ĭ����1 �ض������
 */
ZEND_METHOD(wing_process, run) 
{
	int redirect_output = 1;
	zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &redirect_output);
	
	zval *_output_file = zend_read_property(wing_process_ce, getThis(), "output_file", strlen("output_file"), 0, 0 TSRMLS_CC);
	char *output_file  = Z_STRVAL_P(_output_file);
	zval *_command     = zend_read_property(wing_process_ce, getThis(), "command_line", strlen("command_line"), 0, 0 TSRMLS_CC);
	char *command      = Z_STRVAL_P(_command);


    #ifdef PHP_WIN32
	STARTUPINFO sui;
	PROCESS_INFORMATION *pi=new PROCESS_INFORMATION(); // �������������ӽ��̵���Ϣ
	SECURITY_ATTRIBUTES sa;                            // �����̴��ݸ��ӽ��̵�һЩ��Ϣ



	sa.bInheritHandle = TRUE;                         // �������ӽ��̼̳и����̵Ĺܵ����
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);

	SECURITY_ATTRIBUTES *psa = NULL;
	DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	OSVERSIONINFO osVersion = { 0 };
	osVersion.dwOSVersionInfoSize = sizeof(osVersion);
	if (GetVersionEx(&osVersion))
	{
		if (osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)
		{
			psa = &sa;
			dwShareMode |= FILE_SHARE_DELETE;
		}
	}

	HANDLE hConsoleRedirect = CreateFile(
		output_file,
		GENERIC_WRITE,
		dwShareMode,
		psa,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	SetLastError(0);
	ZeroMemory(&sui, sizeof(STARTUPINFO));         // ��һ���ڴ������㣬�����ZeroMemory, �����ٶ�Ҫ����memset

	sui.cb = sizeof(STARTUPINFO);
	sui.dwFlags = STARTF_USESTDHANDLES;
	sui.hStdInput = NULL;//m_hRead;
	sui.hStdOutput = hConsoleRedirect;//m_hWrite;
	sui.hStdError = hConsoleRedirect;//GetStdHandle(STD_ERROR_HANDLE);
	//sui.wShowWindow = SW_SHOW;
	if(!redirect_output)
	sui.dwFlags = STARTF_USESHOWWINDOW;// | STARTF_USESTDHANDLES;;
									 /*if( params_len >0 ) {
									 DWORD byteWrite  = 0;
									 if( ::WriteFile( m_hWrite, params, params_len, &byteWrite, NULL ) == FALSE ) {
									 php_error_docref(NULL TSRMLS_CC, E_WARNING, "write data to process error");
									 }
									 }*/
	if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &sui, pi)) {
		CloseHandle(hConsoleRedirect);
		RETURN_LONG(WING_ERROR_FAILED);
		return;
	}
	CloseHandle(hConsoleRedirect);

	zend_update_property_long(wing_process_ce, getThis(), "process_info_pointer", strlen("process_info_pointer"), (zend_long)pi TSRMLS_CC);
	//zend_update_property_string(wing_process_ce, getThis(), "command_line", strlen("command_line"), command TSRMLS_CC);
	zend_update_property_long(wing_process_ce, getThis(), "process_id", strlen("process_id"), pi->dwProcessId TSRMLS_CC);
	zend_update_property_long(wing_process_ce, getThis(), "thread_id", strlen("thread_id"), pi->dwThreadId TSRMLS_CC);

	RETURN_LONG(pi->dwProcessId);
	#else

    pid_t childpid = fork();

    printf(PHP_PATH);

	if (childpid == 0){
        //child process
        if (file_is_php(command)) {
            if (execl(PHP_PATH, "php", command ,NULL) < 0) {
                //perror("error on exec");
                exit(0);
            }
        } else {
            if (execl("/bin/sh", "sh", "-c", command, NULL) < 0) {
                exit(0);
            }
        }
    }

	zend_update_property_long(wing_process_ce, getThis(), "process_id", strlen("process_id"), (int)childpid TSRMLS_CC);
	RETURN_LONG((int)childpid);
	#endif
}


/**
 * �ȴ����̽���
 * 
 * @param int $timout �ȴ������볬ʱ����ѡ������Ĭ��Ϊ INFINITE�� ��˼Ϊ������ʱ
 * @return int
 */
ZEND_METHOD(wing_process, wait) {

	#ifdef PHP_WIN32
	int timeout = INFINITE;
	#else
	int timeout = 0;
	#endif

	zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &timeout);

    #ifdef PHP_WIN32
	HANDLE process = NULL;
	zval *file = zend_read_property(wing_process_ce, getThis(), "file", strlen("file"), 0, 0 TSRMLS_CC);

	if (is_numeric_string(Z_STRVAL_P(file), Z_STRLEN_P(file), NULL, NULL, 0)) {
		process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, zend_atoi(Z_STRVAL_P(file), Z_STRLEN_P(file)));
	}
	else {
		zval *_pi = zend_read_property(wing_process_ce, getThis(), "process_info_pointer", strlen("process_info_pointer"), 0, 0 TSRMLS_CC);

		PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *)Z_LVAL_P(_pi);
		process = pi->hProcess;
	}

	DWORD wait_result = 0;
	DWORD wait_status = WaitForSingleObject(process, timeout);

	if (wait_status != WAIT_OBJECT_0) {
		RETURN_LONG(wait_status);
	}
	if (GetExitCodeProcess(process, &wait_result) == 0) {
		RETURN_LONG(WING_ERROR_FAILED);
	}

	RETURN_LONG(wait_result);
	#else
	 int status;
	 	zval *process_id = zend_read_property(wing_process_ce, getThis(), "process_id", strlen("process_id"), 0, 0 TSRMLS_CC);
        pid_t childpid = Z_LVAL_P(process_id);
        printf("---%d\r\n",childpid);
	    pid_t epid = waitpid(childpid, &status, timeout);
	    /*
	    ret=waitpid(-1,NULL,WNOHANG | WUNTRACED);
        如果我们不想使用它们，也可以把options设为0，如：
        ret=waitpid(-1,NULL,0);
        WNOHANG 若pid指定的子进程没有结束，则waitpid()函数返回0，不予以等待。若结束，则返回该子进程的ID。
        WUNTRACED 若子进程进入暂停状态，则马上返回，但子进程的结束状态不予以理会。
        WIFSTOPPED(status)宏确定返回值是否对应与一个暂停子进程。
	    */
	RETURN_LONG(epid);

	#endif
}

/**
 * ��ȡ����id
 *
 * @return int
 */
ZEND_METHOD(wing_process, getProcessId) {
	zval *process_id = zend_read_property(wing_process_ce, getThis(), "process_id", strlen("process_id"), 0, 0 TSRMLS_CC);
	RETURN_ZVAL(process_id,0,0);
}


/**
 * ��ȡ�߳�id
 *
 * @return int
 */
ZEND_METHOD(wing_process, getThreadId) {
	zval *thread_id = zend_read_property(wing_process_ce, getThis(), "thread_id", strlen("thread_id"), 0, 0 TSRMLS_CC);
	RETURN_ZVAL(thread_id,0,0);
}


/**
 * ��ȡ������������
 *
 * @return string
 */
ZEND_METHOD(wing_process, getCommandLine)
{
	zval *file = zend_read_property(wing_process_ce, getThis(), "file", strlen("file"), 0, 0 TSRMLS_CC);

	if (is_numeric_string(Z_STRVAL_P(file), Z_STRLEN_P(file), NULL, NULL, 0)) {
		#ifdef PHP_WIN32
		PROCESSINFO *item = new PROCESSINFO();
		WingQueryProcessByProcessID(item, zend_atoi(Z_STRVAL_P(file), Z_STRLEN_P(file)));
		if (item)
		{
			int size = strlen(item->command_line) + 1;
			char *command_line = "";//(char*)emalloc(size);
			memset(command_line, 0, size);
			spprintf(&command_line, size,"%s", item->command_line);
			delete item;
			RETURN_STRING(command_line);
		}
		#endif
	}
	else {

		zval *command_line = zend_read_property(wing_process_ce, getThis(),
			"command_line", strlen("command_line"), 0, 0 TSRMLS_CC);
	
		ZVAL_ZVAL(return_value, command_line, 0, 0);
	}
}

/**
 * ɱ������
 *
 * @return int
 */
ZEND_METHOD(wing_process, kill)
{

	zval *file     = zend_read_property(wing_process_ce, getThis(), "file", strlen("file"), 0, 0 TSRMLS_CC);
	#ifdef PHP_WIN32
	HANDLE process = NULL;

	//�жϽ��̲����Ƿ�Ϊ������ ��������� ��ͨ������id �򿪽���
	if (is_numeric_string(Z_STRVAL_P(file), Z_STRLEN_P(file), NULL, NULL, 0)) {
		process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, zend_atoi(Z_STRVAL_P(file), Z_STRLEN_P(file)));
	} else {
		zval *_pi = zend_read_property(wing_process_ce, getThis(), "process_info_pointer", strlen("process_info_pointer"), 0, 0 TSRMLS_CC);

		PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *)Z_LVAL_P(_pi);
		process = pi->hProcess;
	}

	//��ֹ����
	if (!TerminateProcess(process, 0)) {

		RETURN_LONG(WING_ERROR_FAILED);
		return;
	}
	RETURN_LONG(WING_ERROR_SUCCESS);
	#endif
}

/**
 * ��ȡ����ռ�õ����ǵ��ڴ��С
 * 
 * @return int  
 */
ZEND_METHOD(wing_process, getMemory) {

	zval *file     = zend_read_property(wing_process_ce, getThis(), "file", strlen("file"), 0, 0 TSRMLS_CC);
	#ifdef PHP_WIN32
	HANDLE process = NULL;

	if (is_numeric_string(Z_STRVAL_P(file), Z_STRLEN_P(file), NULL, NULL, 0)) {
		process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, zend_atoi(Z_STRVAL_P(file), Z_STRLEN_P(file)));
	} else {
		zval *_pi = zend_read_property(wing_process_ce, getThis(), "process_info_pointer", strlen("process_info_pointer"), 0, 0 TSRMLS_CC);

		PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *)Z_LVAL_P(_pi);
		process = pi->hProcess;
	}

	if (!process) {
		RETURN_LONG(0);
	}

	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(process, &pmc, sizeof(pmc));
	RETURN_LONG(pmc.WorkingSetSize);
	#endif
}

/**
 * ��ȡ��ǰ����id
 *
 * @return int
 */
ZEND_METHOD(wing_process, getCurrentProcessId) {
    #ifdef PHP_WIN32
	ZVAL_LONG(return_value, GetCurrentProcessId());
	#else
    zval *process_id = zend_read_property(wing_process_ce, getThis(), "process_id", strlen("process_id"), 0, 0 TSRMLS_CC);
	RETURN_ZVAL(process_id,0,0);
	#endif
}


static zend_function_entry wing_process_methods[] = {
	ZEND_ME(wing_process, __construct,NULL,ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	ZEND_ME(wing_process, __destruct, NULL,ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
	ZEND_ME(wing_process, wait,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, run,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, getProcessId,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, getThreadId,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, getCommandLine,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, kill,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, getMemory,  NULL,ZEND_ACC_PUBLIC)
	ZEND_ME(wing_process, getCurrentProcessId,  NULL,ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{
	    NULL,NULL,NULL
	}
};


PHP_MINIT_FUNCTION(wing_process)
{

    #ifdef PHP_WIN32
	PHP_PATH = (char*)malloc(MAX_PATH);
	memset(PHP_PATH, 0, MAX_PATH);
	GetModuleFileName(NULL, PHP_PATH, MAX_PATH);
	#else
	PHP_PATH = getCommandPath("php");
	printf(PHP_PATH);
	printf("\r\n");
	#endif

	REGISTER_STRING_CONSTANT("WING_PROCESS_PHP",     PHP_PATH,                 CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT("WING_PROCESS_VERSION", PHP_WING_PROCESS_VERSION, CONST_CS | CONST_PERSISTENT );

	zend_class_entry _wing_process_ce;
	//INIT_CLASS_ENTRY(_wing_process_ce, "wing_process", wing_process_methods);
	INIT_NS_CLASS_ENTRY(_wing_process_ce, "wing", "wing_process", wing_process_methods);
	wing_process_ce = zend_register_internal_class(&_wing_process_ce TSRMLS_CC);
	
	zend_declare_property_string(wing_process_ce, "file", strlen("file"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(wing_process_ce, "output_file", strlen("output_file"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(wing_process_ce, "process_info_pointer", strlen("process_info_pointer"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	//zend_declare_property_long(wing_process_ce, "redirect_handler", strlen("redirect_handler"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(wing_process_ce, "command_line", strlen("command_line"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(wing_process_ce, "process_id", strlen("process_id"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(wing_process_ce, "thread_id", strlen("thread_id"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);


	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(wing_process)
{
	
	if (PHP_PATH) {
		free(PHP_PATH);
	}
	return SUCCESS;
}

PHP_RINIT_FUNCTION(wing_process)
{
#if defined(COMPILE_DL_WING_PROCESS) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(wing_process)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(wing_process)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "wing_process support", "enabled");
	php_info_print_table_end();
}


const zend_function_entry wing_process_functions[] = {
//	PHP_FE(wing_process_wait,NULL)
//	PHP_FE(wing_create_process_ex,NULL)
	//PHP_FE(alarm, NULL)
	PHP_FE_END	/* Must be the last line in wing_process_functions[] */
};

zend_module_entry wing_process_module_entry = {
	STANDARD_MODULE_HEADER,
	"wing_process",
	wing_process_functions,
	PHP_MINIT(wing_process),
	PHP_MSHUTDOWN(wing_process),
	PHP_RINIT(wing_process),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(wing_process),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(wing_process),
	PHP_WING_PROCESS_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_WING_PROCESS
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(wing_process)
#endif

