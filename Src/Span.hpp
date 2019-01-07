#pragma once

template <typename T>
class Span
{
public:
	inline Span()
		: m_data(nullptr), m_size(0) { }
	
	template <typename Container, class = std::enable_if_t<std::is_class<Container>::value>>
	inline Span(Container& c)
		: m_data(c.data()), m_size(c.size()) { }
	
	template <class = std::enable_if<std::is_const_v<T>>>
	inline Span(const Span<std::remove_const_t<T>>& c)
		: m_data(c.data()), m_size(c.size()) { }
	
	template <size_t N>
	inline Span(T (&array)[N])
		: m_data(array), m_size(N) { }
	
	inline Span(T* data, size_t size)
		: m_data(data), m_size(size) { }
	
	inline T* begin() const
	{ return m_data; }
	inline T* end() const
	{ return m_data + m_size; }
	
	inline size_t size() const
	{ return m_size; }
	inline T* data() const
	{ return m_data; }
	
	inline size_t SizeBytes() const
	{ return m_size * sizeof(T); }
	
	inline T& At(size_t index) const
	{
		if (index >= m_size)
			throw std::range_error("Span index out of range");
		return m_data[index];
	}
	
	inline T& operator[](size_t index) const
	{
		return m_data[index];
	}
	
	inline bool Empty() const
	{
		return m_size == 0;
	}
	
	template <typename It>
	inline void CopyTo(It output)
	{
		std::copy_n(m_data, m_size, output);
	}
	
private:
	T* m_data;
	size_t m_size;
};
