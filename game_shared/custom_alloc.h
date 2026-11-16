#include <new>
/************************************
	simple memory allocator
	by BUzer
************************************/

template <class T>
class MemBlock
{
	typedef struct chunk_s
	{
		int		next;
		T		data;
	} chunk_t;

public:
	MemBlock( int numElements )
	{
		if( numElements < 0 ) numElements = 0;

		// element 0 is the head of the used list
		m_iSIZEOFARRAY = numElements + 1;
		m_iCurrent = 0;
		m_iCount = 0;
		m_pArray = new(std::nothrow) chunk_t[m_iSIZEOFARRAY];

		if( !m_pArray )
		{
			m_iSIZEOFARRAY = 0;	// no storage: Allocate() fails, StartPass() fails
			m_iFirstFree = 0;
			return;
		}

		Clear();
	}

	~MemBlock()
	{
		delete[] m_pArray;
		m_pArray = NULL;
	}

	void Clear( void )
	{
		m_iCurrent = 0;
		m_iCount = 0;

		if( !m_pArray ) return;

		m_pArray[0].next = 0; // 0 means the used list is empty
		m_iFirstFree = 1; // equals m_iSIZEOFARRAY when there are no elements

		for( int i = 1; i < m_iSIZEOFARRAY; ++i )
			m_pArray[i].next = i + 1;
	}

	T *Allocate( void )
	{
		if( !m_pArray || m_iFirstFree == m_iSIZEOFARRAY )
			return NULL;

		int savedFirstFree = m_pArray[m_iFirstFree].next;
		m_pArray[m_iFirstFree].next = m_pArray[0].next;	// move the free cell to
		m_pArray[0].next = m_iFirstFree;				// the head of the used list
		m_iFirstFree = savedFirstFree;					// and drop it from the free list
		m_iCount++;

		return &(m_pArray[m_pArray[0].next].data);
	}

	bool IsClear( void )
	{
		return !m_pArray || m_pArray[0].next == 0;
	}

	int GetCount( void ) const
	{
		return m_iCount;
	}

	bool StartPass( void )
	{
		m_iCurrent = 0;	// start from the list head
		return m_pArray && m_iSIZEOFARRAY > 1;
	}

	T *GetCurrent( void )
	{
		if( !m_pArray ) return NULL;

		int retindex = m_pArray[m_iCurrent].next;
		if( !retindex )
			return NULL;

		return &(m_pArray[retindex].data);
	}

	void MoveNext( void )
	{
		if( !m_pArray ) return;
		m_iCurrent = m_pArray[m_iCurrent].next;
	}

	void DeleteCurrent( void )
	{
		if( !m_pArray ) return;

		int delindex = m_pArray[m_iCurrent].next;
		if( !delindex ) return;	// nothing to delete (never push the list head onto the free list)

		m_pArray[m_iCurrent].next = m_pArray[delindex].next;	// unlink from the used list
		m_pArray[delindex].next = m_iFirstFree;
		m_iFirstFree = delindex;	// push onto the free list
		m_iCount--;
	}

private:
	// owns a raw buffer: no copying
	MemBlock( const MemBlock & );
	MemBlock &operator=( const MemBlock & );

	chunk_t *m_pArray;
	int	m_iSIZEOFARRAY;
	int	m_iCurrent;	// cursor for passes over the list
	int	m_iCount;	// number of used elements
	int	m_iFirstFree;	// head of the free list
};