#pragma once
#include "sqlite/sqlite3.h"
#include <map>
#include <string>
#include <vector>

/*��ȡsqlite���ݿ���h264��֡���ݲ�����Ƶд�����*/

struct sqliteVideoTableParam
{
	//���ݿ�·��
	std::string sqliteDBPath = ".\\h264.db";    
	//����
	std::string TableName    = "mon_video";    

	//�ֶ���
	std::string time_stamp   = "time_stamp";
	std::string file_time    = "file_time";
	std::string frame_no     = "frame_no";
	std::string info         = "info";
	std::string data         = "data";		//blob �洢ͼ��
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
	//sqliteһ��5��ԭ�����ͣ�NULL, INTEGER(1~8byte), REAL��double 8byte��, text�ı��ַ���, BLOB�ڴ��
	std::map<std::string, std::vector<long long>*>		m_mapINTEGER;
	std::map<std::string, std::vector<double>*>			m_mapREAL;
	std::map<std::string, std::vector<std::string>*>	m_mapTEXT;
	std::map<std::string, std::vector<std::string>*>	m_mapBLOB;

	sqlite3*			  m_pSqliteDB;
	sqliteVideoTableParam m_sqliteVideoTableParam;
	videoParam		      m_videoParam;
};