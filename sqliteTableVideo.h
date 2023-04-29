#pragma once
#include "sqlite/sqlite3.h"
#include <map>
#include <string>
#include <vector>

/*读取sqlite数据库中h264的帧数据并将视频写入磁盘*/

struct sqliteVideoTableParam
{
	//数据库路径
	std::string sqliteDBPath = ".\\h264.db";    
	//表名
	std::string TableName    = "mon_video";    

	//字段名
	std::string time_stamp   = "time_stamp";
	std::string file_time    = "file_time";
	std::string frame_no     = "frame_no";
	std::string info         = "info";
	std::string data         = "data";		//blob 存储图像
	std::string key_frame    = "key_frame";
	std::string frame_index  = "frame_index";
	std::string file_index   = "file_index";
};


class sqliteGetVideoTable
{
	struct videoParam
	{
		int frameWidth = 1440;
		int frameHeight = 1440;
		int frameRate = 30;
	};
public:
	sqliteGetVideoTable();
	~sqliteGetVideoTable();

	bool GetDBdata();
	std::vector<std::string>* GetBlob();
	bool WriteH264ToFile(const char* pOutFileName);

private:
	bool ReadStmt(sqlite3_stmt* pStmt);
	bool CloseDB();

public:
	//sqlite一共5种原生类型：NULL, INTEGER(1~8byte), REAL（double 8byte）, text文本字符串, BLOB内存块
	std::map<std::string, std::vector<long long>*>		m_mapINTEGER;
	std::map<std::string, std::vector<double>*>			m_mapREAL;
	std::map<std::string, std::vector<std::string>*>	m_mapTEXT;
	std::map<std::string, std::vector<std::string>*>	m_mapBLOB;

	sqlite3*			  m_pSqliteDB;
	sqliteVideoTableParam m_sqliteVideoTableParam;
	videoParam		      m_videoParam;
};