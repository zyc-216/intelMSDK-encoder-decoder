#include "sqliteTableVideo.h"
#include "GWAVI.h"

sqliteGetVideoTable::sqliteGetVideoTable()
{
	m_pSqliteDB = nullptr;
}

sqliteGetVideoTable::~sqliteGetVideoTable()
{
	for (auto iter = m_mapBLOB.begin(); iter != m_mapBLOB.end(); iter++){
		delete iter->second;
	}

	for (auto iter = m_mapINTEGER.begin(); iter != m_mapINTEGER.end(); iter++){
		delete iter->second;
	}

	for (auto iter = m_mapREAL.begin(); iter != m_mapREAL.end(); iter++){
		delete iter->second;
	}

	for (auto iter = m_mapTEXT.begin(); iter != m_mapTEXT.end(); iter++){
		delete iter->second;
	}

	CloseDB();
}


bool sqliteGetVideoTable::GetDBdata()
{
	bool bRet = false;
	int  res = SQLITE_OK;
	sqlite3_stmt* pStmt = nullptr;
	std::string strSql = {};

	res = sqlite3_open_v2(m_sqliteVideoTableParam.sqliteDBPath.c_str(), &m_pSqliteDB, SQLITE_OPEN_READWRITE, NULL);
	if (SQLITE_OK != res)
		return bRet;
	
	sprintf(const_cast<char*>(strSql.data()), "select * from %s", m_sqliteVideoTableParam.TableName);

	res = sqlite3_prepare_v2(m_pSqliteDB, strSql.c_str(), -1, &pStmt, 0);
	if (SQLITE_OK != res)
		return bRet;

	while (SQLITE_ROW == sqlite3_step(pStmt)) {
		ReadStmt(pStmt);
	}

	sqlite3_finalize(pStmt);
	bRet = true;
	return bRet;
}

std::vector<std::string>* sqliteGetVideoTable::GetBlob()
{
	return m_mapBLOB[m_sqliteVideoTableParam.data];
}

bool sqliteGetVideoTable::WriteH264ToFile(const char* pOutFileName)
{
	bool bRet = false;
	GWAVI aviWriter(pOutFileName, m_videoParam.frameWidth, m_videoParam.frameHeight, 0, "H264", m_videoParam.frameRate, NULL);
	std::vector<std::string>* pvecBlobs = GetBlob();

	for (int i = 0; i < pvecBlobs->size(); ++i)
	{
		aviWriter.AddVideoFrame((unsigned char*)pvecBlobs->at(i).data(), pvecBlobs->at(i).size());
	}

	aviWriter.Finalize();
	bRet = true;
	return bRet;
}

bool sqliteGetVideoTable::CloseDB()
{
	bool bRet = false;

	if (nullptr != m_pSqliteDB) {
		sqlite3_close_v2(m_pSqliteDB);
		m_pSqliteDB = nullptr;
		bRet = true;
	}
	return bRet;
}

bool sqliteGetVideoTable::ReadStmt(sqlite3_stmt* pStmt)
{
	bool bRet = false;
	int iColumnCount = sqlite3_column_count(pStmt);

	for (int iCol = 0; iCol < iColumnCount; ++iCol) 
	{
		std::string strColumnName = sqlite3_column_name(pStmt,iCol);
		int iColType = sqlite3_column_type(pStmt, iCol);
		
		if (SQLITE_INTEGER == iColType) 
		{
			auto iter = m_mapINTEGER.find(strColumnName);
			if (m_mapINTEGER.end() == iter) 
			{
				std::vector<long long>* pvecLL = new std::vector<long long>;
				pvecLL->push_back(sqlite3_column_int64(pStmt, iCol));
				m_mapINTEGER.insert(std::make_pair(strColumnName, pvecLL));
			}
			else
			{
				iter->second->push_back(sqlite3_column_int64(pStmt, iCol));
			}
		}
		else if (SQLITE_FLOAT == iColType)
		{
			auto iter = m_mapREAL.find(strColumnName);
			if (m_mapREAL.end() == iter)
			{
				std::vector<double>* pvecREAL = new std::vector<double>;
				pvecREAL->push_back(sqlite3_column_double(pStmt, iCol));
				m_mapREAL.insert(std::make_pair(strColumnName, pvecREAL));
			}
			else 
			{
				iter->second->push_back(sqlite3_column_double(pStmt, iCol));
			}
		}
		else if(SQLITE_TEXT == iColType) 
		{
			auto iter = m_mapTEXT.find(strColumnName);
			if (m_mapTEXT.end() == iter)
			{
				std::vector<std::string>* pvecTEXT = new std::vector<std::string>;
				pvecTEXT->push_back((const char*)sqlite3_column_text(pStmt, iCol));
				m_mapTEXT.insert(std::make_pair(strColumnName, pvecTEXT));
			}
			else
			{
				iter->second->push_back((const char*)sqlite3_column_text(pStmt, iCol));
			}
		}
		else if(SQLITE_BLOB == iColType) 
		{
			auto iter = m_mapBLOB.find(strColumnName);
			if (m_mapBLOB.end() == iter) 
			{
				std::vector<std::string>* pvecBLOB = new std::vector<std::string>;
				const char* blob = (const char*)sqlite3_column_blob(pStmt, iCol);
				int iBlobLength = sqlite3_column_bytes(pStmt, iCol);
				pvecBLOB->push_back(std::string(blob, iBlobLength));
				m_mapBLOB.insert(std::make_pair(strColumnName, pvecBLOB));
			}
			else 
			{
				const char* blob = (const char*)sqlite3_column_blob(pStmt, iCol);
				int iBlobLength = sqlite3_column_bytes(pStmt, iCol);
				iter->second->push_back(std::string(blob, iBlobLength));
			}
		}
	}
	bRet = true;
	return bRet;
}
