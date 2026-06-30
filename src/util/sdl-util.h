#ifndef SDLUTIL_H
#define SDLUTIL_H

#ifdef MKXPZ_RETRO
#  include "filesystem.h"
#else
#  include <SDL_atomic.h>
#  include <SDL_thread.h>
#  include <SDL_rwops.h>
#endif // MKXPZ_RETRO

#include <cstring>
#include <iostream>
#include <unistd.h>

template<typename O, typename R, size_t bufSize = 248, size_t pbSize = 8>
class RWBuf : public std::streambuf
{
public:
	RWBuf(O ops)
	    : ops(ops)
	{
		char *end = buf + bufSize + pbSize;
		setg(end, end, end);
	}

private:
	int_type underflow()
	{
		if (!ops)
			return traits_type::eof();

		if (gptr() < egptr())
			return traits_type::to_int_type(*gptr());

		char *base = buf;
		char *start = base;

		if (eback() == base)
		{
			std::memmove(base, egptr() - pbSize, pbSize);
			start += pbSize;
		}

		size_t n = R::read(ops, start, bufSize - (start - base));
		if (n == 0)
			return traits_type::eof();

		setg(base, start, start + n);

		return underflow();
	}

	O ops;
	char buf[bufSize+pbSize];
};

#ifdef MKXPZ_RETRO
struct AtomicFlag
{
	AtomicFlag() : atom(false) {}

	void set()
	{
		atom = true;
	}

	void clear()
	{
		atom = false;
	}

	void wait() {}

	void reset()
	{
		set();
	}

	operator bool() const
	{
		return atom;
	}

private:
	mutable bool atom;
};

class PHYSFSRead
{
public:
	static size_t read(std::shared_ptr<struct FileSystem::File> ops, void *buf, size_t size)
	{
		return std::max((PHYSFS_sint64)0, PHYSFS_readBytes(ops->get_read(), buf, size));
	}
};

typedef RWBuf<std::shared_ptr<struct FileSystem::File>, PHYSFSRead> PHYSFSRWBuf;
#else
struct AtomicFlag
{
	AtomicFlag()
	{
		clear();
	}

	void set()
	{
		SDL_AtomicSet(&atom, 1);
	}

	void clear()
	{
		SDL_AtomicSet(&atom, 0);
	}
    
    void wait()
    {
        while (SDL_AtomicGet(&atom)) {}
    }
    
    void reset()
    {
        wait();
        set();
    }

	operator bool() const
	{
		return SDL_AtomicGet(&atom);
	}

private:
	mutable SDL_atomic_t atom;
};

template<class C, void (C::*func)()>
int __sdlThreadFun(void *obj)
{
	(static_cast<C*>(obj)->*func)();
	return 0;
}

template<class C, void (C::*func)()>
SDL_Thread *createSDLThread(C *obj, const std::string &name = std::string())
{
	return SDL_CreateThread((__sdlThreadFun<C, func>), name.c_str(), obj);
}

/* On Android, SDL_RWFromFile always opens files from inside
 * the apk asset folder even when a file with same name exists
 * on the physical filesystem. This wrapper attempts to open a
 * real file first before falling back to the assets folder */
static inline
SDL_RWops *RWFromFile(const char *filename,
                      const char *mode)
{
	FILE *f = fopen(filename, mode);

	if (!f)
		return SDL_RWFromFile(filename, mode);

	return SDL_RWFromFP(f, SDL_TRUE);
}

inline bool readFileSDL(const char *path,
                        std::string &out)
{
	SDL_RWops *f = RWFromFile(path, "rb");

	if (!f)
		return false;

	long size = SDL_RWsize(f);
	size_t back = out.size();

	out.resize(back+size);
	size_t read = SDL_RWread(f, &out[back], 1, size);
	SDL_RWclose(f);

	if (read != (size_t) size)
		out.resize(back+read);

	return true;
}

class SDLRead
{
public:
	static size_t read(SDL_RWops *ops, void *buf, size_t size)
	{
		return SDL_RWread(ops, buf, 1, size);
	}
};

typedef RWBuf<SDL_RWops *, SDLRead> SDLRWBuf;

class SDLRWStream
{
public:
	SDLRWStream(const char *filename,
	            const char *mode)
	    : ops(RWFromFile(filename, mode)),
	      buf(ops),
	      s(&buf)
	{}

	~SDLRWStream()
	{
		if (ops)
			SDL_RWclose(ops);
	}

	operator bool() const
	{
		return ops != 0;
	}

	std::istream &stream()
	{
		return s;
	}

private:
	SDL_RWops *ops;
	SDLRWBuf buf;
	std::istream s;
};
#endif // MKXPZ_RETRO

#endif // SDLUTIL_H
