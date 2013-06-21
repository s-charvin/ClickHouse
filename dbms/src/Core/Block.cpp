#include <iterator>

#include <DB/Core/Exception.h>
#include <DB/Core/ErrorCodes.h>

#include <DB/Core/Block.h>


namespace DB
{


Block::Block(const Block & other)
{
	*this = other;
}


Block & Block::operator= (const Block & other)
{
	data = other.data;
	rebuildIndexByPosition();
	index_by_name.clear();
	
	for (IndexByName_t::const_iterator it = other.index_by_name.begin(); it != other.index_by_name.end(); ++it)
	{
		Container_t::iterator value = data.begin();
		std::advance(value, std::distance(const_cast<Block&>(other).data.begin(), it->second));
		index_by_name[it->first] = value;
	}
	
	return *this;
}


void Block::rebuildIndexByPosition()
{
	index_by_position.resize(data.size());
	size_t pos = 0;
	for (Container_t::iterator it = data.begin(); it != data.end(); ++it, ++pos)
		index_by_position[pos] = it;
}


void Block::insert(size_t position, const ColumnWithNameAndType & elem)
{
	if (position > index_by_position.size())
		throw Exception("Position out of bound in Block::insert(), max position = "
			+ toString(index_by_position.size()), ErrorCodes::POSITION_OUT_OF_BOUND);

	if (position == index_by_position.size())
	{
		insert(elem);
		return;
	}
		
	Container_t::iterator it = data.insert(index_by_position[position], elem);
	rebuildIndexByPosition();
	index_by_name[elem.name] = it; 
}


void Block::insert(const ColumnWithNameAndType & elem)
{
	Container_t::iterator it = data.insert(data.end(), elem);
	rebuildIndexByPosition();
	index_by_name[elem.name] = it;
}


void Block::insertUnique(const ColumnWithNameAndType & elem)
{
	if (index_by_name.end() == index_by_name.find(elem.name))
		insert(elem);
}


void Block::erase(size_t position)
{
	if (position >= index_by_position.size())
		throw Exception("Position out of bound in Block::erase(), max position = "
			+ toString(index_by_position.size()), ErrorCodes::POSITION_OUT_OF_BOUND);
		
	Container_t::iterator it = index_by_position[position];
	index_by_name.erase(index_by_name.find(it->name));
	data.erase(it);
	rebuildIndexByPosition();
}


void Block::erase(const String & name)
{
	IndexByName_t::iterator index_it = index_by_name.find(name);
	if (index_it == index_by_name.end())
		throw Exception("No such name in Block::erase(): '"
			+ name + "'", ErrorCodes::NOT_FOUND_COLUMN_IN_BLOCK);
	
	Container_t::iterator it = index_it->second;
	index_by_name.erase(index_it);
	data.erase(it);
	rebuildIndexByPosition();
}


ColumnWithNameAndType & Block::getByPosition(size_t position)
{
	if (position >= index_by_position.size())
		throw Exception("Position " + toString(position)
			+ " is out of bound in Block::getByPosition(), max position = "
			+ toString(index_by_position.size() - 1)
			+ ", there are columns: " + dumpNames(), ErrorCodes::POSITION_OUT_OF_BOUND);
		
	return *index_by_position[position];
}


const ColumnWithNameAndType & Block::getByPosition(size_t position) const
{
	if (position >= index_by_position.size())
		throw Exception("Position " + toString(position)
			+ " is out of bound in Block::getByPosition(), max position = "
			+ toString(index_by_position.size() - 1)
			+ ", there are columns: " + dumpNames(), ErrorCodes::POSITION_OUT_OF_BOUND);
		
	return *index_by_position[position];
}


ColumnWithNameAndType & Block::getByName(const std::string & name)
{
	IndexByName_t::const_iterator it = index_by_name.find(name);
	if (index_by_name.end() == it)
		throw Exception("Not found column " + name + " in block. There are only columns: " + dumpNames()
			, ErrorCodes::NOT_FOUND_COLUMN_IN_BLOCK);

	return *it->second;
}


const ColumnWithNameAndType & Block::getByName(const std::string & name) const
{
	IndexByName_t::const_iterator it = index_by_name.find(name);
	if (index_by_name.end() == it)
		throw Exception("Not found column " + name + " in block. There are only columns: " + dumpNames()
			, ErrorCodes::NOT_FOUND_COLUMN_IN_BLOCK);

	return *it->second;
}


bool Block::has(const std::string & name) const
{
	return index_by_name.end() != index_by_name.find(name);
}


size_t Block::getPositionByName(const std::string & name) const
{
	IndexByName_t::const_iterator it = index_by_name.find(name);
	if (index_by_name.end() == it)
		throw Exception("Not found column " + name + " in block. There are only columns: " + dumpNames()
			, ErrorCodes::NOT_FOUND_COLUMN_IN_BLOCK);

	return std::distance(const_cast<Container_t &>(data).begin(), it->second);
}


size_t Block::rows() const
{
	size_t res = 0;
	for (Container_t::const_iterator it = data.begin(); it != data.end(); ++it)
	{
		size_t size = it->column->size();

		if (res != 0 && size != res)
			throw Exception("Sizes of columns doesn't match: "
				+ data.begin()->name + ": " + toString(res)
				+ ", " + it->name + ": " + toString(size)
				, ErrorCodes::SIZES_OF_COLUMNS_DOESNT_MATCH);

		res = size;
	}

	return res;
}


size_t Block::rowsInFirstColumn() const
{
	if (data.empty() || data.front().column.isNull())
		return 0;

	return data.front().column->size();
}


size_t Block::columns() const
{
	return data.size();
}


size_t Block::bytes() const
{
	size_t res = 0;
	for (size_t i = 0; i < columns(); ++i)
		res += getByPosition(i).column->byteSize();

	return res;
}


std::string Block::dumpNames() const
{
	std::stringstream res;
	for (Container_t::const_iterator it = data.begin(); it != data.end(); ++it)
	{
		if (it != data.begin())
			res << ", ";
		res << it->name;
	}
	return res.str();
}


Block Block::cloneEmpty() const
{
	Block res;

	for (Container_t::const_iterator it = data.begin(); it != data.end(); ++it)
		res.insert(it->cloneEmpty());

	return res;
}


ColumnsWithNameAndType Block::getColumns() const
{
	return ColumnsWithNameAndType(data.begin(), data.end());
}


NamesAndTypesList Block::getColumnsList() const
{
	NamesAndTypesList res;

	for (Container_t::const_iterator it = data.begin(); it != data.end(); ++it)
		res.push_back(NameAndTypePair(it->name, it->type));

	return res;
}


bool blocksHaveEqualStructure(const Block & lhs, const Block & rhs)
{
	size_t columns = lhs.columns();
	if (rhs.columns() != columns)
		return false;

	for (size_t i = 0; i < columns; ++i)
	{
		const IDataType & lhs_type = *lhs.getByPosition(i).type;
		const IDataType & rhs_type = *rhs.getByPosition(i).type;

		if (lhs_type.getName() != rhs_type.getName())
			return false;
	}

	return true;
}

}
