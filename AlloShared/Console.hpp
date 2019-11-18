#pragma once

#include <iostream>
#include <thread>

#include "CommandHandler.hpp"

class Console
{
public:
    Console(CommandHandler& commandHandler);
    
    void start();
    
private:
    class ReadlineStreambuf : public std::streambuf
    {
    public:
        ReadlineStreambuf(std::streambuf& outStream);
        
    protected:
        virtual std::streamsize xsputn(const char* s, std::streamsize count);
        virtual int overflow(int c = EOF);
        
    private:
        std::streambuf& outStream;
    };
    
    static char* generator(const char* text, int state);
    static char** completion(const char * text , int start, int end);
    
    void runLoop();
    std::thread runThread;
    ReadlineStreambuf readlineStreambuf;
    
    static std::vector<CommandHandler::Command> const* currentCommands;
    CommandHandler& commandHandler;
};
