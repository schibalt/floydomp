#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <cmath>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <iomanip>
#include <omp.h>
#include <sstream>
#include <unistd.h>
#include <string>
#include <vector>
#include <sys/time.h>

#ifdef _WIN32
#include <Windows.h>
#endif

using std::cin;
using std::ifstream;
using std::istream_iterator;
using std::istringstream;
using std::string;
using std::stringstream;
using std::vector;
using std::ofstream;

const int MAX_CHARS_PER_LINE = 512, MAX_TOKENS_PER_LINE = 20;
// read an entire line into memory
char buf[MAX_CHARS_PER_LINE];
const char* const DELIMITER = " ";
vector<string> cities;
int nCities, threads;

//let dist be a |V| × |V| array of minimum distances initialized to ∞ (infinity)'
int** dist;
int** next;

bool bidirectional;
string filename;

uint64_t GetTimeMs64() {

#ifdef WIN32
	/* Windows */
	FILETIME ft;
	LARGE_INTEGER li;

	/* Get the amount of 100 nano seconds intervals elapsed
	 * since January 1, 1601 (UTC) and copy it
	 * to a LARGE_INTEGER structure. */
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	uint64_t ret = li.QuadPart;
	ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
	ret /= 10000; // From 100 nano seconds (10^-7)

	return ret;
#else
	/* Linux */
	struct timeval tv;

	gettimeofday(&tv, NULL);

	uint64_t ret = tv.tv_usec;

	/* Convert from micro seconds (10^-6) to milliseconds (10^-3) */
//	ret /= 1000;
	/* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
	ret += (tv.tv_sec * 1000);

	return ret;
#endif
}

string Path(int i, int j) {
	if (dist[i][j] == INT_MAX)
		return "no path";    //no path

	string sintermediate = " ";
	sintermediate = next[i][j];
	int intermediate = next[i][j];

	if (intermediate == -1)
		return " ";   // the direct edge from i to j gives the shortest path
	else {
		string pathiint = Path(i, intermediate);
		string pathintj = Path(intermediate, j);
		stringstream sstm;
		sstm << pathiint << intermediate << pathintj;

		return sstm.str();
	}
}

vector<string> split(string const & ilooput) {
	istringstream buffer(ilooput);
	vector<string> ret;

	copy(istream_iterator<string>(buffer), istream_iterator<string>(),
			back_inserter(ret));
	return ret;
}

vector<int> GetPath(int source, int destination) {
	string spath = Path(source, destination);
	//printf ("\loopath: %i%ls%i\n", source, wspath.c_str(), destination);
	//const string spath (spath.begin(), spath.end());
	vector<int> pathvector;

	pathvector.push_back(source);
	vector<string> intermediatepath = split(spath);

	for (vector<string>::iterator it = intermediatepath.begin();
			it != intermediatepath.end(); ++it) {
		int city = atoi((*it).c_str());
		pathvector.push_back(city);
	}

	pathvector.push_back(destination);

	return pathvector;
}

void InitializeArrays() {

	// create a file-reading object
	ifstream fin;
	fin.open(filename.c_str());   // open a file

	if (!fin.good()) {
		printf("File for graph data couldn't be found or couldn't be open.");
		exit(0); // exit if file not found
	}

	fin.getline(buf, MAX_CHARS_PER_LINE);
	istringstream(buf) >> nCities;
	cities.push_back(string(buf, buf + strlen(buf)));

	for (int cityidx = nCities; cityidx > 0; cityidx--) {
		fin.getline(buf, MAX_CHARS_PER_LINE);
		string city = string(buf, buf + strlen(buf));
		transform(city.begin(), city.end(), city.begin(), ::tolower);
		cities.push_back(city);
		//string svcity = cities[nCities - city + 1];
		//for (unsigned i = 0; i < svcity.length(); ++i)
		//printf ("%c\n", svcity.at (i));
	}
	//inst arrays;

	//let dist be a |V| × |V| array of minimum distances initialized to ∞ (infinity)
	dist = new int*[nCities + 1];
	next = new int*[nCities + 1];

	for (int i = nCities; i > 0; i--)     //for each edge (u,v)
			{
		dist[i] = new int[nCities + 1]; //dist[u][v] ← w(u,v)  // the weight of the edge (u,v)
		next[i] = new int[nCities + 1];

		for (int j = nCities; j > 0; j--) {
			if (i == j)
				dist[i][j] = 0;
			else
				dist[i][j] = INT_MAX;
			next[i][j] = -1;
		}
	}
	//arrays initialized

	//parse initial city distances
	while (!fin.eof()) {
		fin.getline(buf, MAX_CHARS_PER_LINE);

		if (buf[0] == '-')
			break;

		istringstream iss(buf);
		int sourceCity = -1, destCity = -1, distance = INT_MAX;

		for (int subdex = 3; subdex > 0; subdex--) //iterate thice for each substring
				{
			//(there's actually an additional substring that's either empty or whitespace)
			string sub;
			iss >> sub;
			string wsdestcity;
			string wssourcecity;

			switch (subdex) {
			case 1:
				distance = atoi(sub.c_str());
				//printf ("distance: %i\n", distance);
				break;
			case 2:
				destCity = atoi(sub.c_str());
				//printf ("destination: ");
				wsdestcity = cities[destCity];
				//for (unsigned i = 0; i < wsdestcity.length(); ++i)
				//printf ("%c\n", wsdestcity.at (i));
				break;
			case 3:
				sourceCity = atoi(sub.c_str());
				//printf ("\nsource: ");
				wssourcecity = cities[sourceCity];
				//for (unsigned i = 0; i < wssourcecity.length(); ++i)
				//printf ("%c\n", wssourcecity.at (i));
				break;
			default:
				printf("this isn't the substring index you're looking for\n");
			}   //switch
		}   //for

		if (distance != INT_MAX)     //inf dist edges already initialized
		{
			//only overwrite edge vals is they're explicitly finite
			//(safety from overwriting zero-val edges from each vertex to itself)
			dist[sourceCity][destCity] = distance;

			if (bidirectional)
				dist[destCity][sourceCity] = distance;
		}
	} //while
	fin.close();
	//initial distances initialized
}

void CleanUpArrays() {
	for (int x = 1; x <= nCities; x++) {
		delete[] dist[x];
		delete[] next[x];
	}
	delete[] dist;
	delete[] next;
}

int FindCity() {
	cin >> buf;
	int city = atoi(buf);

	if (city < 1 || city > nCities) {
		printf("\nEnter a valid id.\n");
		return FindCity();
	} else {
		printf("\n%s is city %d.\n", cities[city].c_str(), city);
		return city;
	}
}

//doesn't work on UNIX
int FindCityWindows() {
	cin >> buf;
	string city = string(buf, buf + strlen(buf));
	transform(city.begin(), city.end(), city.begin(), ::tolower);

	vector<string>::iterator iter = find(cities.begin(), cities.end(), city);
	size_t index = distance(cities.begin(), iter);
	if (index == cities.size()) {
		printf("\nThere isn't any information available about %s.",
				city.c_str());
		printf("\nEnter the name of the city again.\n");
		return FindCity();
	} else {
		//printf ("\n%s is city %d.\n", city.c_str(), index);
		return index;
	}
}

void PathLoop() {
	string buffer;

	while (buffer[0] != 'n') {
		printf("\n");
		for (int city = nCities; city > 0; city--)
			printf("%02i  %s\n", nCities - city + 1,
					cities[nCities - city + 1].c_str());

		int source = 0;
		int destination = 0;
		while (source == destination) {
			printf("\nEnter the id of the origin city.\n");

			source = FindCity();

			printf("\nEnter the id of the destination city.\n");

			destination = FindCity();
		}

		vector<int> pathvector = GetPath(source, destination); //then get intermediate path

		printf("\nShortest path from %s to %s is %d.\n", cities[source].c_str(),
				cities[destination].c_str(), dist[source][destination]);

		for (unsigned int i = 0; i < pathvector.size() - 1; i++) {
			int from = pathvector[i];
			int to = pathvector[i + 1];
			printf("    %s to %s:\t%i\n", cities[from].c_str(),
					cities[to].c_str(), dist[from][to]);
			//cout << "    " << cities[from] << " to " << cities[to] << ":\t" << dist[from][to] << "\n";
		}
		printf("\nRun again? (y/n)\n");

		cin >> buffer;

		while (buffer[0] != 'y' && buffer[0] != 'n')
			cin >> buffer;
	} //test loop
}

int main(int argc, char* argv[]) {

	filename = argv[1];
	if (atoi(argv[2]))
		bidirectional = 1;
	else
		bidirectional = 0;

	threads = atoi(argv[3]);
	bool rundialog = atoi(argv[4]);

	InitializeArrays();

	if (threads > 0)
		omp_set_num_threads(threads);

	bool knowompthreadcount = false;
	int ompthreadcount = -1;

	uint64_t runstarttime = GetTimeMs64();

#pragma omp parallel for
	for (int k = 1; k <= nCities; k++) {

		if (!knowompthreadcount)
			ompthreadcount = omp_get_num_threads();
		for (int i = 1; i <= nCities; i++) {

			for (int j = 1; j <= nCities; j++) {

				int polldist;
				int kthcolumn = dist[i][k];
				int kthrow = dist[k][j];

				//the sum of a negative weight edge and an infinite weight edge only
				//seems less than an infinite weight edge because of the use of int data type.
				//in principle the sum is still infinite.
				if (kthrow == INT_MAX || kthcolumn == INT_MAX)
					polldist = INT_MAX;
				else
					polldist = dist[i][k] + dist[k][j];

#pragma omp critical
				if (polldist < dist[i][j]) {
					dist[i][j] = polldist;
					next[i][j] = k;
				}
			} //j
		} //i
	}        //k

	uint64_t runendtime = GetTimeMs64();
	uint64_t runtime = runendtime - runstarttime;

	ofstream outfile;
	outfile.open("floydompexperiments.txt", ofstream::app);
	char buffer[50];
	sprintf(buffer,
			"\nrun time for %i threads with thread arg %i:  %" PRIu64 " μs\n",
			ompthreadcount, threads, runtime);
	printf("%s", buffer);
	outfile << buffer;
	outfile.close();

	if (rundialog)
		PathLoop();
	CleanUpArrays();

	exit(EXIT_SUCCESS);
}
