#include <stdio.h>
#include <bits/stdc++.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>
#define CACHE_CAPACITY 10
#define MAX_CLIENTS 10
#define CURR_NEG_VALUE -2
#define BUFFER_SIZE 2048
//Struct for HTTP request format containing filename and hostname
struct Http_request{
	char filename[1024];
	char hostname[1024];
};

struct LRU_entry{
	std::time_t exp_tsEpoch;
	std::string expire_date;
	std::string url;
	int lu;
	int val;
};

/*Structure to represent the request parameters*/
struct req_detail{
	int number;
    int cache_id;
	char filename[1024];
	bool temp_flag;
	bool isExpired;
};

struct client_request{
  int client_request_id;
  bool mode;
  std::string URL;
  };


//Performing updates to LRU
void updateLRUCache(LRU_entry LRU[], int block)
{
	if(LRU[block].lu==1)
		return;
	else{
		LRU[block].lu = 1;
		for(int i=0; i<CACHE_CAPACITY; i++)
		{
			if(i!=block)
			{
				LRU[i].lu++;
				if(LRU[i].lu>CACHE_CAPACITY)
					LRU[i].lu = CACHE_CAPACITY;
			}
		}
	}
	return;
}

//Getting index of least recently used cache entry
int getLRU(LRU_entry LRU[])
{
	int max_index = CACHE_CAPACITY;
	int ret = -1;
	for(max_index=CACHE_CAPACITY; max_index>0; max_index--){
		for(int i=0; i<CACHE_CAPACITY; i++)
		{
			if(LRU[i].lu==max_index){
				if(LRU[i].val==0)
				{
					ret = i;
					LRU[ret].val=CURR_NEG_VALUE;
					max_index=0;
					break;
				}
			}
		}
	}
	return ret;
}

// Check if the request was already cached
//returns -1 if not found else return the cache block index
int ifCached(LRU_entry LRU[], std::map<std::string,int> req_cache_id, std::string req_url)
{
	if(req_cache_id.find(req_url)==req_cache_id.end()){
		return -1;
	}

	if(req_url == LRU[req_cache_id[req_url]].url){
		return req_cache_id[req_url];
	}
	return -1;
}


// Decoding the host name and the file name by parsing through the HTTP request 
struct Http_request * retrieve_http_request(char *buffer)
{
    int index = 0, i = 0, j = 0;
	struct Http_request *req = (struct Http_request *)malloc(sizeof(struct Http_request));

	while(buffer[index++] != ' ');
	req->filename[0] = '/';

	if(buffer[index+8] == '\r'){
		if(buffer[index+9] == '\n')
		{
			req->filename[1] = '\0';
		}
	}
	else
	{
		while(buffer[index] != ' ')
		req->filename[i++] = buffer[index++];
		req->filename[i] = '\0';
	}
	index++;
	while(buffer[index++] != ' ');
	while(buffer[index] != '\r')
	req->hostname[j++] = buffer[index++];
	req->hostname[j] = '\0';
    return req;
}