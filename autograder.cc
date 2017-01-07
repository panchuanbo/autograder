/* AutoGrader
 * ----------
 * Compiles and collects the output from user programs
 * and checks to see if they match the specified output.
 * Motivation: School used an autograder and thought it
 * would be a cool side-project to mimic it using the new
 * things I've learned :D
 * (Obviously) not complete/robust right now, but thought
 * I'd post it for now...
 */
 
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream
#include <unistd.h>     // fork, dup2, pipe, getopt_long
#include <getopt.h>     // getopt_long & constants
#include <sys/wait.h>   // waitpid
#include <sys/stat.h>   // for stat
#include <string>       // std::string
#include <string.h>     // strerror (debugging)
#include <vector>       // std::vector
#include <regex>        // comparison

using namespace std;

// Constants
static int kSuccessfulCompile = 0;
static string kCompilationFailedStatus = "Compilation Failed... Aborting...";
static string kProgramFailedToRunStatus = "Program Failed to Run... Aborting...";
static string kYourOutput = "Your Output:\n------------";
static string kActualOutput = "Actual Output:\n--------------";

/* Function: closeAll
 * ------------------
 * Utility function that closes all
 * the file descriptors
 */
inline void closeAll(int fds[]) {
    close(fds[0]); close(fds[1]);
}

/* Function: checkProgramStatus
 * ----------------------------
 * Checks to see if the program exited correctly
 * Currently, just forces the entire checker program
 * to close if the child doesn't end correctly 
 * (will use real error handling in future versions)
 */
void checkProgramStatus(int status, string failureMessage) {
    if (!WIFEXITED(status) || WEXITSTATUS(status) != kSuccessfulCompile) {
        cout << failureMessage << endl;
        exit(-1);
    }
}

/* Function: compileJavaFile
 * -------------------------
 * Simple system call to compile the program.
 * Nothing robust as of now.
 */
void compileJavaFile() {
    cout << "Compiling Java Files..." << endl;
    int status = system("javac *.java");
    checkProgramStatus(status, kCompilationFailedStatus);
}

/* Function: sendInputToProgram
 * ----------------------------
 * Will eventually pass some data to the program, but
 * right now, it's a no-op.
 */
void sendInputToProgram(int fd) {
    // no-op for now
    close(fd);
}

/* Function: runProgram
 * --------------------
 * Runs the program to completion. Sends input to the
 * program if necessary. Uses fork to run the child program
 * and pipe to direct text to/from the child.
 * Nothing robust as of now, but will add once all core
 * functionality is in place.
 */
int runProgram(const string &mainClass) {
    int sendInput[2];
    int getOutput[2];
    pipe(sendInput);
    pipe(getOutput);
    
    sendInputToProgram(sendInput[1]);
    
    pid_t pid = fork();
    if (pid == 0) {
        // in child process
        dup2(sendInput[0], STDIN_FILENO);
        dup2(getOutput[1], STDERR_FILENO); //yea, I want stderr
        dup2(getOutput[1], STDOUT_FILENO);
        closeAll(sendInput);
        closeAll(getOutput);
        const char *commands[] = { "java", mainClass.c_str(), NULL };
        execvp(commands[0], const_cast<char **>(commands));
        
        return -1;
    }
    close(sendInput[0]);
    close(getOutput[1]);
    
    int status;
    waitpid(pid, &status, 0);
    
    checkProgramStatus(status, kProgramFailedToRunStatus);
    
    cout << "Finished Executing Program" << endl;
    
    return getOutput[0];
}

/* Function: getLineFromFd
 * -----------------------
 * Reads an entire line from the file descriptor
 */
bool getLineFromFd(int fd, string &str) {
    str = "";
    char buffer[2];
    ssize_t numRead;
    
    // I know this is not the best way...
    // I'll find something else eventually
    // so I'll keep the do-while until then... (lol)
    do {
        numRead = read(fd, buffer, 1);
        if (numRead > 0) {
            if (buffer[0] == '\n') return true;
            str += buffer[0];
        } else if (numRead == 0) { // we're now done
            return false;
        } else if (numRead < 0) {
            cout << "Error Reading from File Descriptor! Aborting..." << endl;
            exit(-1);
        }
    } while (numRead > 0);
    
    return true;
}

/* Function: readOutput
 * --------------------
 * Gets all the output from the program by reading
 * in from the file descriptor line-by-line
 */
void readOutput(int fd, vector<string> &output) {
    // let's wait and get some output shall we?
    while (true) {
        string line;
        bool res = getLineFromFd(fd, line);
        output.push_back(line);
        if (!res) break;
    }
    
    cout << "Finished Reading Input" << endl;
    close(fd);
}

/* Function: compareOutput
 * -----------------------
 * Compares user output to the correct output.
 */
bool compareOutput(const vector<string> &userOutput, const vector<string> &solution) {
    if (userOutput.size() != solution.size()) return false;
    for (size_t i = 0; i < userOutput.size(); i++) {
        if (userOutput[i] != solution[i]) return false;
    }
    return true;
}

/* Function: printVector
 * ---------------------
 * Prints the vector. If there is a mismatch, everything is
 * printed. Otherwise, only the first 3 lines are printed.
 */
bool printVector(const vector<string> &output, bool all, const string &headerText) {
    if (headerText.length() != 0) cout << headerText << endl;;
    // if all = false, print at most three line
    size_t printLen = (all) ? output.size() : 3;
    for (int i = 0; i < 3 && i < output.size(); i++) {
        cout << output[i] << endl;
    }
    
    if (!all && printLen < output.size()) cout << "..." << endl;
}

/* checkFileExists
 * ---------------
 * Checks to see if a file exists before opening.
 */
inline bool fileExists(const std::string& file) {
  struct stat buffer;   
  return (stat(file.c_str(), &buffer) == 0); 
}

/* loadSolutionFile
 * ----------------
 * Load solutions file.
 */
void loadSolutionFile(string solFile, vector<string> &sol) {
    if (fileExists) {
        ifstream fin(solFile.c_str());
        string line;
        while (getline(fin, line)) sol.push_back(line);
    } else {
        cout << "Can't find solution file... Aborting..." << endl;
        exit(-1);
    }
}

/* Function: printArgReqs
 * ----------------------
 * Print argument requirements if the arguments list is
 * nonexistent/malformed
 */
void printArgReqs() {
    exit(0);
}

/* Function: main
 * --------------
 * Start of the checker program. 
 * TODO: Finish the Following:
 * - Get terminal input
 * - Specify correct output file
 * - Add in options for output file
 * - Add in some (real) autograding stuff.
 * - Add in different modes (e.g., output to file and diff)
 */
int main(int argc, char *argv[]) {
    if (argc < 3) printArgReqs();
    
    string mainClass = "";
    string solFile = "";
    // 0 = no_argument
    // 1 = required_argument
    // 2 = optional_argument
    struct option options[] = {
        {"main", 1, NULL, 'm'},
        {"solution", 1, NULL, 's'}
    };
    
    while (true) {
        int ch = getopt_long(argc, argv, "m:s:", options, NULL);
        if (ch == -1) break;
        switch (ch) {
        case 'm':
            mainClass = string(optarg);
            break;
        case 's':
            solFile = string(optarg);
            break;
        default:
            printArgReqs();
            break;
        }
    }
    vector<string> output;      // program output
    vector<string> solution;    // solution output
    
    loadSolutionFile(solFile, solution);
    
    compileJavaFile();
    int outputFd = runProgram(mainClass);
    readOutput(outputFd, output);
    bool result = compareOutput(output, solution);
    
    if (result) {
        cout << "[OK]: Matched" << endl
             << "-------------" << endl;
    } else {
        cout << "[NOT OK]: MISMATCH" << endl
             << "------------------" << endl;
    }
    
    printVector(solution, !result, kActualOutput);
    printVector(output, !result, kYourOutput);
    
    return 0;
}