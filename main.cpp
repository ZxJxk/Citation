#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "httplib.h"
#include <nlohmann/json.hpp>


using json = nlohmann::json;
using namespace std;

// Base class for citations
class Citation {
public:
    string id;
    virtual string getFormattedString() const = 0;
    virtual void fetchAdditionalInfo(httplib::Client& client) = 0;
};

// Derived class for books
class Book : public Citation {
public:
    string isbn;
    string author;
    string title;
    string publisher;
    string year;

    // Constructor
    Book(const string& id, const string& isbn) : isbn(isbn) {
        this->id = id;
    }

    // Fetch additional book info from API
    void fetchAdditionalInfo(httplib::Client& client) override {
        string url = "/isbn/" + httplib::detail::encode_url(isbn);
        auto res = client.Get(url.c_str());
        if (res && res->status == 200) {
            json response = json::parse(res->body);
            author = response["author"].get<string>();
            title = response["title"].get<string>();
            publisher = response["publisher"].get<string>();
            year = response["year"].get<string>();
        }
        else {
            cerr << "Failed to fetch book info for ISBN: " << isbn << endl;
            exit(1);
        }
    }

    // Return formatted string
    string getFormattedString() const override {
        return "[" + id + "] book: " + author + ", " + title + ", " + publisher + ", " + year;
    }
};

// Derived class for web pages
class Webpage : public Citation {
public:
    string url;
   // string url = "https://www.baidu.com";
    string title;

    // Constructor
    Webpage(const string& id, const string& url) : url(url) {
        this->id = id;
    }

    // Fetch webpage title from API
   
    void fetchAdditionalInfo(httplib::Client& client) override {
         cout<< url<<endl;
	 string encodedUrl = httplib::detail::encode_url(url);
	 cout<<encodedUrl<<endl;
        string apiUrl = encodedUrl;
	//string apiUrl = "/title/" + encodedUrl;
        auto res = client.Get(apiUrl.c_str());
	cout<< res->body<<endl;
        if (res && res->status == 200) {
            json response = json::parse(res->body);
            title = response["title"].get<string>();
        }
        else {
            cerr << "Failed to fetch webpage title for URL: " << url << endl;
            exit(1);
        }
    }

    // Return formatted string
    string getFormattedString() const override {
        return "[" + id + "] webpage: " + title + ". Available at " + url;
    }
};

// Derived class for articles
class Article : public Citation {
public:
    string title;
    string author;
    string journal;
    string year;
    int volume;
    int issue;

    // Constructor
    Article(const string& id, const json& data) {
        this->id = id;
        title = data["title"].get<string>();
        author = data["author"].get<string>();
        journal = data["journal"].get<string>();
        year = data["year"].get<string>();
        volume = data["volume"].get<int>();
        issue = data["issue"].get<int>();
    }

    // Return formatted string
    string getFormattedString() const override {
        return "[" + id + "] article: " + author + ", " + title + ", " + journal + ", " + year + ", " + to_string(volume) + ", " + to_string(issue);
    }

    void fetchAdditionalInfo(httplib::Client& client) override {
        // No additional info needed for articles
    }
};
//const char* citationFilePath = "/path/to/citation.json";


// Function to parse citation JSON file
  map<string, unique_ptr<Citation>> parseCitations(const string& citationFilePath, httplib::Client& client) {
    map<string, unique_ptr<Citation>> citations;

    ifstream file(citationFilePath);
    if (!file.is_open()) {
        cerr << "Failed to open citation file: " << citationFilePath << endl;
        exit(1);
    }

    json citationData;
    file >> citationData;
    file.close();

    for (const auto& citation : citationData["citations"]) {
        string id = citation["id"].get<string>();
        string type = citation["type"].get<string>();

        if (type == "book") {
            string isbn = citation["isbn"].get<string>();
            unique_ptr<Book> book = make_unique<Book>(id, isbn);
            book->fetchAdditionalInfo(client);
            citations[id] = move(book);
        }
        else if (type == "webpage") {
            string url = citation["url"].get<string>();
            unique_ptr<Webpage> webpage = make_unique<Webpage>(id, url);
            webpage->fetchAdditionalInfo(client);
            citations[id] = move(webpage);
        }
        else if (type == "article") {
            unique_ptr<Article> article = make_unique<Article>(id, citation);
            citations[id] = move(article);
        }
    }

    return citations;
}

// Function to parse article file and extract references
vector<string> parseArticle(const string& articleFilePath, string& articleContent) {
    ifstream file(articleFilePath);
    if (!file.is_open()) {
        cerr << "Failed to open article file: " << articleFilePath << endl;
        exit(1);
    }

    stringstream buffer;
    buffer<<file.rdbuf();
    articleContent = buffer.str();
    file.close();

    vector<string> references;
    size_t pos = 0;
    while ((pos = articleContent.find('[', pos)) != string::npos) {
        size_t endPos = articleContent.find(']', pos);
        if (endPos == string::npos) {
            cerr << "Malformed article file: unmatched '['" << endl;
            exit(1);
        }
        string reference = articleContent.substr(pos + 1, endPos - pos - 1);
        references.push_back(reference);
        pos = endPos + 1;
    }

    return references;
}

// Main function
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " OPTIONS input_file "<< endl;
        return 1;
    }

    // Parse command line arguments
    string citationFilePath;
   // const char* citationFilePath = "/path/to/citation.json";
    string outputFilePath;
    string articleFilePath = argv[argc - 1];
    // 定义输入文章文件的路径
   // std::string inputFilePath = "/path/to/input/article.txt";  // 请根据你的实际文件路径修改

    // 定义文献合集的路径
   //std::string citationFilePath = "/path/to/citation.json";  // 请根据你的实际文件路径修改
    string baseUrl = "http://docman.lcpu.dev";

    for (int i = 1; i < argc - 1; i += 2) {
        string option = argv[i];
        if (option == "-c") {
            citationFilePath = argv[i + 1];
        }
        else if (option == "-o") {
            outputFilePath = argv[i + 1];
        }
        else if (option == "--base_url") {
            baseUrl = argv[i + 1];
        }
        else {
            cerr << "Unknown option: " << option << endl;
            return 1;
        }
    }

    // Validate required options
    if (citationFilePath.empty()) {
        cerr << "Error: Citation file path is required." << endl;
        return 1;
    }

    // Initialize HTTP client
    httplib::Client client("47.108.166.87");

    // Parse citations and fetch additional info
    map<string, unique_ptr<Citation>> citations = parseCitations(citationFilePath, client);

    // Parse article file and extract references
    string articleContent;
    vector<string> references = parseArticle(articleFilePath, articleContent);

    //Generate reference list;
    std::stringstream referenceList{};
    referenceList << "\n\nReferences:\n";
    for (const string& ref : references) {
        if (citations.find(ref) != citations.end()) {
            referenceList<<citations[ref]->getFormattedString() << "\n";
        }
        else {
            cerr << "Reference ID not found in citations: [" << ref << "]" << endl;
            return 1;
        }
    }

    // Add reference list to article content
    articleContent += referenceList.str();

    // Output result
    if (outputFilePath.empty()) {
        cout << articleContent << endl;
    }
    else {
        ofstream outFile(outputFilePath);
        if (!outFile.is_open()) {
            cerr << "Failed to open output file: " << outputFilePath << endl;
            return 1;
        }
        outFile << articleContent;
        outFile.close();
    }

    return 0;
}
