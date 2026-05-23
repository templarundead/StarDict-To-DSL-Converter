#include "stardict.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
    #include <winsock.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include <string>
#include <algorithm>
#include <vector>
#include <omp.h>

#include "utils.h"

using namespace std;

// Структура для метаданных слова, чтобы распараллелить цикл
struct WordEntry {
    string word;
    unsigned int offset;
    unsigned int size;
};

static string getFileName(const string& s) {
  char sep = '/';
#ifdef _WIN32
  sep = '\\';
#endif

  size_t i = s.rfind(sep, s.length());
  if (i != string::npos) {
    return(s.substr(i+1, s.length() - i));
  }
  return("");
}

string getDictName(const char* ifoFileName) {
  string dictName = "";
  
  char *buffer;
  FILE * pFile = fopen(ifoFileName, "r");
  if (!pFile) return dictName;
  fseek(pFile, 0, SEEK_END);
  long lSize = ftell(pFile);
  rewind(pFile);

  buffer = (char*) malloc(sizeof(char) *lSize);
  size_t result = fread(buffer, 1, lSize, pFile);
  fclose(pFile);
  
  string bufStr(buffer, lSize);
  free(buffer);
  string namePref = "bookname=";
  int pos1 = bufStr.find(namePref);
  
  if (pos1 != string::npos) {
    int pos2 = bufStr.find("\n", pos1 + namePref.size());
    pos1 = pos1 + namePref.size();
    dictName = bufStr.substr(pos1, pos2-pos1);
  }
  
  return dictName;
}

void convert_stardict_to_dsl(const char* ifoFileName) {
  string dictName = getDictName(ifoFileName);
  
  string idxFileName = ifoFileName;
  idxFileName.replace(idxFileName.length()-sizeof("ifo")+1, sizeof("ifo")-1, "idx");
  struct stat idx_stats;
  if (stat(idxFileName.c_str(), &idx_stats) == -1) {
    printf("File does not exist: '%s'\n", idxFileName.c_str());
    return;
  }
  
  string dictFileName = ifoFileName;
  dictFileName.replace(dictFileName.length()-sizeof("ifo")+1, sizeof("ifo")-1, "dict");
  struct stat dict_stats;
  
  if (stat(dictFileName.c_str(), &dict_stats) == -1) {
    printf("File does not exist: '%s'\n", dictFileName.c_str());
    return;
  }
  
  char *idxbuffer = (char *) malloc(idx_stats.st_size);
  char *idxbuffer_end = idxbuffer + idx_stats.st_size;
  FILE *idxFile;
  idxFile = fopen(idxFileName.c_str(), "rb");
  size_t read_idx = fread(idxbuffer, 1, idx_stats.st_size, idxFile);
  fclose(idxFile);
  
  char *dictbuffer = (char *) malloc(dict_stats.st_size);
  FILE *dictFile;
  dictFile = fopen(dictFileName.c_str(), "rb");
  size_t read_dict = fread(dictbuffer, 1, dict_stats.st_size, dictFile);
  fclose(dictFile);
  
  // === Векторизация индекса (быстрый однопоточный проход)
  vector<WordEntry> entries;
  char *p = idxbuffer;
  
  while (p < idxbuffer_end) {
    WordEntry entry;
    int wordlen = strlen(p);
    entry.word = string(p, wordlen);
    p += wordlen + 1;
    
    entry.offset = ntohl(*reinterpret_cast<unsigned int *>(p));
    p += sizeof(unsigned int);
    
    entry.size = ntohl(*reinterpret_cast<unsigned int *>(p));
    p += sizeof(unsigned int);
    
    entries.push_back(entry);
  }
  
  size_t total_words = entries.size();
  vector<string> output_chunks(total_words);

  // === ПАРАЛЛЕЛЬНАЯ ОБРАБОТКА (Тяжелый многопоточный блок)
  // Распределяем обработку HTML строк по всем доступным ядрам процессора
  #pragma omp parallel for schedule(dynamic, 100)
  for (size_t i = 0; i < total_words; ++i) {
    const WordEntry& entry = entries[i];
    char* data = dictbuffer + entry.offset;
    
    string dataStr(data, entry.size);
    dataStr = Utils::html_to_dsl(dataStr);
    
    // Формируем кусок DSL-статьи в памяти
    output_chunks[i] = entry.word + "\n  " + dataStr + "\n\n";
  }
  
  // === ЗАПИСЬ НА ДИСК (Однопоточный быстрый сброс упорядоченных данных)
  string outFileName = Utils::replace_string(ifoFileName, "\\.(ifo|IFO)$", "") + ".dsl";
  outFileName.replace(outFileName.length()-sizeof("ifo")+1, sizeof("ifo")-1, "dsl");
  printf("Writing to file: '%s'\n", outFileName.c_str());
  
  FILE *txtFile = fopen(outFileName.c_str(), "w");
  if (txtFile) {
    string dictSourceLang = "English";
    string dictTargetLang = "English";
    string headerStr = "#NAME \"" + dictName + "\"\n#INDEX_LANGUAGE \"" + dictSourceLang + "\"\n#CONTENTS_LANGUAGE \"" + dictTargetLang + "\"\n\n";
    fwrite(headerStr.c_str(), 1, headerStr.size(), txtFile);
    
    for (size_t i = 0; i < total_words; ++i) {
      fwrite(output_chunks[i].c_str(), 1, output_chunks[i].size(), txtFile);
    }
    fclose(txtFile);
  }
  
  free(idxbuffer);
  free(dictbuffer);
}
