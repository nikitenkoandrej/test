#pragma once

#include <istream>
#include <ostream>
#include <set>
#include <list>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <future>
#include <mutex>
#include <utility>
#include <future>
#include <mutex>
#include "profile.h"

using namespace std;

template <typename T>
class Synchronized {
public:
  explicit Synchronized(T initial = T())
    : value(move(initial))
  {
  }

  struct Access {
    T& ref_to_value;
    lock_guard<mutex> guard;
  };

  Access GetAccess() {
    return {value, lock_guard(m)};
  }

private:
  T value;
  mutex m;
};


class InvertedIndex {
public:
  void Add(const string& document);
  vector<pair<size_t, size_t>> Lookup(const string& word) const;

  const string& GetDocument(size_t id) const {
    return docs[id];
  }

  size_t size(){
	  return docs.size();
  }

private:

  vector<vector<pair<size_t, size_t>>> index;
  map<string, size_t> orgIndex;
  vector<string> docs;
};

class SearchServer {
public:
  SearchServer() = default;
  explicit SearchServer(istream& document_input);
  void UpdateDocumentBase(istream& document_input);
  int UDBSingle(istream& document_input);
  void AddQueriesStream(istream& query_input, ostream& search_results_output);
  int AQSSingleThread(istream& query_input, ostream& search_results_output);
  size_t size(){
	  return index.size();
  }

private:
  bool databaseRdy;
  mutex mtxIndex;
  mutex mtxAsync;
  InvertedIndex index;
  vector<future<int>> futures;
  deque<pair<istream&, ostream&>> querries;
  size_t ctr;
};
