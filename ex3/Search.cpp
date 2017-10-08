#include "MapReduceFramework.h"
#include <stdio.h>
#include <iostream>
#include <dirent.h>
#include <sstream>

using namespace std;

#define NUM_OF_THREADS 10
#define ADV 1


class searchK1: public k1Base
{
public:
    char* path;
    bool operator<(const k1Base &other) const
    {
        return (this->path < ((const searchK1&)other).path);
    }
    searchK1(char* path) : path(path){};
};

class searchK2: public k2Base
{
public:
    string fileName;
    bool operator<(const k2Base &other) const
    {
        return (this->fileName < ((const searchK2&)other).fileName);
    }
    searchK2(string fileName) : fileName(fileName){};
};

class searchK3: public k3Base
{
public:
    string fileName;
    ~searchK3(){}
    bool operator<(const k3Base &other) const
    {
        return (this->fileName < ((const searchK3&)other).fileName);
    }
    searchK3(string fileName) : fileName(fileName){};

};

class searchV1 : public v1Base
{
public:
    char* searchTerm;
    searchV1(char* searchTerm) : searchTerm(searchTerm){}
};

class searchV2 : public v2Base
{
public:

    char* searchTerm;
    searchV2(char* searchTerm) : searchTerm(searchTerm){}
};

class searchV3 : public v3Base
{
public:

    bool exist;
    searchV3(bool exist) : exist(exist){};
};




class mapAndReduce : public MapReduceBase
{
    /*
     * the map function. recieves a key and value. where the key is the path
     * name and the value is the search term.
     * it creates k2 which is the files (or dir, or anything in the folder..)
     * and v2 which is the same as v1. and inserts them into a list. using
     * Emit2.
     */
    void Map(const k1Base *const key, const v1Base *const val) const
    {
        int j = 0;
        searchK1* keySer = (searchK1*) key;
        searchV1* valSer = (searchV1*) val;
        DIR* dir;
        string s = valSer->searchTerm;
        struct dirent* ent;
        list <char*> files;
        dir=opendir(keySer->path);
        if(dir)
        {
            ent = readdir(dir);
            while (ent)
            {

                stringstream stream;
                string name;
                stream << ent->d_name;
                stream >> name;
                j++;
                searchV2 *v2 = new searchV2(valSer->searchTerm);
                searchK2 *k2 = new searchK2(name);
                Emit2(k2, v2);
                ent = readdir(dir);
            }
        }
        closedir(dir);
    }

    /*
     * the Reduce function recieves a k2Base key, and a v2Base list of values
     * for each of them. in our implementation, there is usually not much
     * importence for the list itself. the key is a folder name. and the value
     * list is the search term.the only way we'll have more then 1 value in a
     * list is we have 2 folders with the same name.
     * the Reduce function, checks if the search value (or values, but they're
     * alwayes the same) exists in the filename. if so it adds it with a value
     * of true using Emit3. Otherwise, it adds it with false value. (the key
     * remains the same).
     */
    void Reduce(const k2Base *const key, const V2_LIST &vals) const
    {
        searchK2* keySer = (searchK2*) key;
        searchK3* k3 = new searchK3(keySer->fileName);
        searchV3* v3 = new searchV3(false);
        std :: string str = keySer->fileName;
        for(v2Base* val:vals)
        {
            if (str.find(((searchV2*)val)->searchTerm) != std::string::npos)
            {
                v3->exist = true;
                break;
            }
        }
        Emit3(k3,v3);
    }
};


int main(int argc, char *argv[])
{
    IN_ITEMS_LIST ourList;
    mapAndReduce ourFunc;
    OUT_ITEMS_LIST outputList;
    list<OUT_ITEM>::iterator it;
    list<IN_ITEM>::iterator it2;
    list<OUT_ITEM>::iterator it3;


    //number of arguments is not satisfactory.
    if (argc <= 1)
    {
        cerr << "Usage:<substring to search><folders,"
                " separated by space>"<<endl;
        exit(0);
    }


    char* search = argv[1];
    searchV1* v1 = new searchV1(search);
    for (int i=2;i<argc;i++)
    {
        searchK1* k1 = new searchK1(argv[i]);
        IN_ITEM item = std::make_pair(k1,v1);
        ourList.push_back(item);
    }
    outputList = runMapReduceFramework(ourFunc, ourList, NUM_OF_THREADS);

    //printing the keys in k3, that has a value that show they do fit
    //with the search term.
    it = outputList.begin();
    searchV3* tempVal;
    searchK3* tempKey;
    for(int i=0; i<outputList.size(); i++)
    {
        tempKey = (searchK3*) it->first;
        tempVal = (searchV3*) it->second;
        if (tempVal->exist)
        {
            printf("%s\n",tempKey->fileName.c_str());
        }
        advance(it,ADV);

    }

    //freeing the list of pairs INPAIR.
    searchV1* tempV1;
    searchK1* tempK1;
    searchV3* tempV3;
    searchK3* tempK3;
    it2 = ourList.begin();
    it3 = outputList.begin();
    for(int i=0; i<ourList.size();i++)
    {
        tempK1 = (searchK1*) it2->first;
        tempV1 = (searchV1*) it2->second;
        delete(tempK1);
        delete(tempV1);
        advance(it2,ADV);
    }
    for(int i = 0; i<outputList.size(); i++)
    {
        tempK3 = (searchK3*) it3->first;
        tempV3 = (searchV3*) it3->second;
        delete(tempK3);
        delete(tempV3);
        advance(it3,ADV);
    }

    return 0;
}
