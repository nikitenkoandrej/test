#include "search_server.h"
#include "iterator_range.h"
#include "test_runner.h"
#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>
#include <chrono>


using namespace std::chrono;


vector<string> SplitIntoWords(const string& line) {
  istringstream words_input(line);
  return {istream_iterator<string>(words_input), istream_iterator<string>()};
}

SearchServer::SearchServer(istream& document_input) {
  databaseRdy = 0;
  UpdateDocumentBase(document_input);
}

int SearchServer::UDBSingle(istream& document_input) {
  InvertedIndex new_index;

  for (string current_document; getline(document_input, current_document); ) {
    new_index.Add(move(current_document));
  }
  lock_guard<mutex> guard(mtxIndex);
  index = move(new_index);
  return 1;
}

void SearchServer::UpdateDocumentBase(istream& document_input) {

		  lock_guard<mutex> guard(mtxAsync);
		  if(!databaseRdy){
			  UDBSingle(document_input);
			  databaseRdy = 1;
		  }
		  else{
	      futures.push_back(async([&document_input, this] {
	    	   return UDBSingle(document_input);}));
		  }

}

int SearchServer::AQSSingleThread(
  istream& query_input, ostream& search_results_output
) {


  vector<pair<size_t, size_t>> docid_count(50000);
  while(!databaseRdy)
      		  ;
  for (string current_query; getline(query_input, current_query); ) {
    const auto words = SplitIntoWords(current_query);
    docid_count.resize(this->size());

    vector<pair<size_t, size_t>> result;
    for (const auto& word : words) {
    	{
    	lock_guard<mutex> guard(mtxIndex);
    	result = move(index.Lookup(word));
    	}
    	//guard.~lock_guard();
      for (const pair<size_t, size_t>& docid : result) {
    	//cout << docid << " - " << docid_count << endl;
        docid_count[docid.first].first = docid.first;
        docid_count[docid.first].second += docid.second;

      }

    }

    partial_sort(
      begin(docid_count), begin(docid_count) + min<size_t>(5, this->size()),
      end(docid_count),
      [](pair<size_t, size_t> lhs, pair<size_t, size_t> rhs) {
        int64_t lhs_docid = lhs.first;
        auto lhs_hit_count = lhs.second;
        int64_t rhs_docid = rhs.first;
        auto rhs_hit_count = rhs.second;
        return make_pair(lhs_hit_count, -lhs_docid) > make_pair(rhs_hit_count, -rhs_docid);
      }
    );



    search_results_output << current_query << ':';
    for (auto [docid, hitcount] : Head(docid_count, 5)) {
    	if(hitcount > 0){
		  search_results_output << " {"
			<< "docid: " << docid << ", "
			<< "hitcount: " << hitcount << '}';
    	}
    }
    search_results_output << endl;
    docid_count.clear();
  }
  return 1;
}
void SearchServer::AddQueriesStream(
  istream& query_input, ostream& search_results_output
) {
	/*AQSSingleThread(query_input, search_results_output);*/


	{
	  lock_guard<mutex> guard(mtxAsync);
	  //querries.push_back({query_input.width(), search_results_output});
	  //if(futures.size() < 4)
	  futures.push_back(async(launch::async, &SearchServer::AQSSingleThread, this, ref(query_input), ref(search_results_output)));
	}
	this_thread::sleep_for(chrono::microseconds(1000));
	  /*
	  if(!futures.empty())
      for (auto& f : futures) {
          f.wait();
        }*/
}

void InvertedIndex::Add(const string& document) {

  docs.push_back(document);
  const size_t docid = docs.size() - 1;
  for (const auto& word : SplitIntoWords(document)) {
	auto res = orgIndex.insert(make_pair(word, index.size()));
	size_t i = res.first->second; //word -> i-я строка
	if(res.second){   // слова нет
		index.push_back({}); // + строка
		index[i].push_back(make_pair(docid, 1)); //добавим док
	}
	else{            // слово есть

		//cout << i <<*(prev(index[i].end())) << prev(index[i].end())->second << word << "increm" << endl;
		if(prev(index[i].end())->first == docid){
			(prev(index[i].end())->second)++;  //увеличим для тек дока на 1
		}
		else
			index[i].push_back(make_pair(docid, 1));

	}
	//cout << word << index[i] << endl;
  }
}

  vector<pair<size_t, size_t>> InvertedIndex::Lookup(const string& word) const {
  if (auto it = orgIndex.find(word); it != orgIndex.end()) {
    return index[it->second];
  } else {
    return {};
  }
}
