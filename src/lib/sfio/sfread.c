#include	"sfhdr.h"

/*	Read n bytes from a stream into a buffer
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
ssize_t sfread(reg Sfio_t* f, Void_t* buf, reg size_t n)
#else
ssize_t sfread(f,buf,n)
reg Sfio_t*	f;	/* read from this stream. 	*/
Void_t*		buf;	/* buffer to read into		*/
reg size_t	n;	/* number of bytes to be read. 	*/
#endif
{
	reg uchar	*s, *begs;
	reg ssize_t	r;
	reg int		local, justseek;

	SFMTXSTART(f,-1);

	GETLOCAL(f,local);
	justseek = f->bits&SF_JUSTSEEK; f->bits &= ~SF_JUSTSEEK;

	/* release peek lock */
	if(f->mode&SF_PEEK)
	{	if(!(f->mode&SF_READ) )
			SFMTXRETURN(f, -1);

		if(f->mode&SF_GETR)
		{	if(((uchar*)buf + f->val) != f->next &&
			   (!f->rsrv || f->rsrv->data != (uchar*)buf) )
				SFMTXRETURN(f, -1);
			f->mode &= ~SF_PEEK;
			SFMTXRETURN(f, 0);
		}
		else
		{	if((uchar*)buf != f->next)
				SFMTXRETURN(f, -1);
			f->mode &= ~SF_PEEK;
			if(f->mode&SF_PKRD)
			{	/* actually read the data now */
				f->mode &= ~SF_PKRD;
				if(n > 0)
					n = (r = read(f->file,f->data,n)) < 0 ? 0 : r;
				f->endb = f->data+n;
				f->here += n;
			}
			f->next += n;
			f->endr = f->endb;
			SFMTXRETURN(f, n);
		}
	}

	s = begs = (uchar*)buf;
	for(;; f->mode &= ~SF_LOCK)
	{	/* check stream mode */
		if(SFMODE(f,local) != SF_READ && _sfmode(f,SF_READ,local) < 0)
		{	n = s > begs ? s-begs : -1;
			SFMTXRETURN(f, n);
		}

		SFLOCK(f,local);

		if((r = f->endb - f->next) > 0) /* has buffered data */
		{	if(r > (ssize_t)n)
				r = (ssize_t)n;
			if(s != f->next)
				memcpy(s, f->next, r);
			f->next += r;
			s += r;
			n -= r;
		}

		if(n <= 0)	/* all done */
			break;

		/* refill buffer */
		if((f->flags&SF_STRING) || (f->bits&SF_MMAP) )
		{	if(SFFILBUF(f,-1) <= 0)
				break;
		}
		else
		{	f->next = f->endb = f->data;

			/* cases where exact IO is desirable */
			if(SFDIRECT(f,n) ||
			   (f->extent <  0 && (f->flags&SF_SHARE) ) ||
			   (f->extent >= 0 && (f->here+n) >= f->extent ) )
				r = (ssize_t)n;
			else if(justseek && n <= f->iosz && f->iosz <= f->size)
				r = f->iosz;	/* limit buffering */
			else	r = f->size;	/* full buffering */

			/* if read almost full size, then just do it direct */
			if(r > (ssize_t)n && (r - r/8) <= (ssize_t)n)
				r = (ssize_t)n;

			if(r == (ssize_t)n) /* read directly to user's buffer */
			{	if((r = SFRD(f,s,r,f->disc)) >= 0)
				{	s += r;
					break;
				}
			}
			else if(SFRD(f,f->next,r,f->disc) == 0) /* eof */
				break;
		}
	}

	SFOPEN(f,local);
	r = s-begs;
	SFMTXRETURN(f, r);
}
