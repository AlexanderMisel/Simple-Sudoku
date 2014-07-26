#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

std::ofstream output("Sudoku.log");

typedef unsigned bitfield;

static const bitfield maskMax = 512;
static const bitfield allSet = 511;

enum difficulty { DEFAULT, EASY, MEDIUM, DIFFICULT, EVIL };

// Returns the size of the set
static unsigned bitCount(bitfield bits)
{
    unsigned result = 0;
    bitfield mask = 1;

    while(mask != maskMax)
    {
        if (bits & mask)
            result++;
        mask *= 2;
    }

    return result;
}

// Returns a bitfield representing {num}
static inline bitfield bitFor(unsigned num)
{
    return 1 << (num - 1);
}

class BlankList
{
public:
    BlankList()
    {
        for (unsigned i = 0; i < 9; ++i)
            rows[i] = cols[i] = allSet;
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 3; ++j)
                blocks[i][j] = allSet;
    }

    void elim(unsigned i, unsigned j, unsigned n)
    {
        bitfield bit = bitFor(n);
        rows[i] &= ~bit;
        cols[j] &= ~bit;
        blocks[i/3][j/3] &= ~bit;
    }

    void cancel(unsigned i, unsigned j, unsigned n)
    {
        bitfield bit = bitFor(n);
        rows[i] |= bit;
        cols[j] |= bit;
        blocks[i/3][j/3] |= bit;
    }

    bitfield possible(unsigned i, unsigned j)
    {
        return rows[i] & cols[j] & blocks[i/3][j/3];
    }
private:
    bitfield rows[9], cols[9];
    bitfield blocks[3][3];
};

class Board
{
public:
    unsigned backtrack_count;

    Board()
        : remains(81)
    {
        for (unsigned i = 0; i < 9; ++i)
            for (unsigned j = 0; j < 9; ++j)
            {
                matrix[i][j] = 0;
                memory[i][j] = allSet;
            }
    }
    Board(unsigned seed)
        : remains(81)
    {
        for (unsigned i = 0; i < 9; ++i)
            for (unsigned j = 0; j < 9; ++j)
            {
                matrix[i][j] = 0;
                memory[i][j] = allSet;
            }

        std::srand(seed);
        _random_fill();
    }
    void set(unsigned row, unsigned col, unsigned val, bool advanced = false)
    {
        matrix[row][col] = val;
        if (matrix[row][col])
        {
            Blank.elim(row, col, val);
            --remains;
        }
        if (advanced)
            _update(row, col);
    }
    unsigned unset(unsigned row, unsigned col)
    {
        unsigned val = matrix[row][col];
        if (val)
        {
            Blank.cancel(row, col, val);
            ++remains;
        }
        matrix[row][col] = 0;
        return val;
    }
    bitfield mask_check(unsigned i, unsigned j, bitfield mask)
    {
        return Blank.possible(i, j) & mask & memory[i][j];
    }
    bitfield house_check(unsigned i, unsigned j, bool advanced = false)
    {
        bitfield house_hidden = allSet;
        unsigned row_base = i / 3 * 3;
        unsigned col_base = j / 3 * 3;
        for (unsigned row = row_base; row < row_base + 3; ++row)
            for (unsigned col = col_base; col < col_base + 3; ++col)
            {
                if ((row == i && col == j) || matrix[row][col])
                    continue;
                house_hidden &= advanced ? ~memory[row][col] : ~Blank.possible(row, col);
            }
        return house_hidden;
    }
    bitfield row_check(unsigned i, unsigned j, bool advanced = false)
    {
        bitfield row_hidden = allSet;
        for (unsigned col = 0; col < 9; ++col)
            if (!matrix[i][col] && col != j)
                row_hidden &= advanced ? ~memory[i][col] : ~Blank.possible(i, col);
        return row_hidden;
    }
    bitfield col_check(unsigned i, unsigned j, bool advanced = false)
    {
        bitfield col_hidden = allSet;
        for (unsigned row = 0; row < 9; ++row)
            if (!matrix[row][j] && row != i)
                col_hidden &= advanced ? ~memory[row][j] : ~Blank.possible(row, j);
        return col_hidden;
    }
    bitfield decide(bitfield & possible, bitfield & house,
                    bitfield & row,      bitfield & col)
    {
        if (bitCount(possible) == 1)
            return possible;

        bitfield check1 = possible & house;
        if (bitCount(check1) == 1)
            return check1;

        bitfield check2 = possible & row;
        if (bitCount(check2) == 1)
            return check2;

        bitfield check3 = possible & col;
        if (bitCount(check3) == 1)
            return check3;

        bitfield check = check1 & check2;
        if (bitCount(check) == 1)
            return check;

        check = check1 & check3;
        if (bitCount(check) == 1)
            return check;

        check = check2 & check3;
        if (bitCount(check) == 1)
            return check;

        check &= check1;
        if (bitCount(check) == 1)
            return check;

        return 0;
    }
    void hidden_fill()
    {
        bool again;
        unsigned count = 0;
        do
        {
            again = false;
            ++count;
            output << "Simple solving attempt " << count
                   << " ..." << std::endl;
            for (unsigned i = 0; i < 9; ++i)
            {
                for (unsigned j = 0; j < 9; ++j)
                {
                    if (matrix[i][j]) continue;
                    bitfield possible = Blank.possible(i, j);
                    bitfield house = house_check(i, j);
                    bitfield row = row_check(i, j);
                    bitfield col = col_check(i, j);
                    bitfield to_check = decide(possible, house, row, col);
                    if (to_check)
                    {
                        set(i, j, numFor(to_check));
                        again = true;
                    }
                }
            }
        }
        while (again);
    }
    void candidate_check(unsigned i, unsigned j)
    {
        bitfield row_locked(0), col_locked(0), row_i(0), col_j(0);
        unsigned row_base = i / 3 * 3;
        unsigned col_base = j / 3 * 3;

        //Locked Candidate Type 1 (Pointing)
        unsigned total_count = 0;
        for (unsigned row = row_base; row < row_base + 3; ++row)
        {
            if (row == i)
            {
                for (unsigned col = col_base; col < col_base + 3; ++col)
                {
                    if (matrix[row][col])
                        continue;
                    row_i |= memory[row][col];
                }
            }
            else
            {
                for (unsigned col = col_base; col < col_base + 3; ++col)
                {
                    if (matrix[row][col])
                        continue;
                    row_locked |= memory[row][col];
                    ++total_count;
                }
            }
        }
        if (total_count && total_count == bitCount(row_locked))
            memory[i][j] &= ~row_locked;
        bitfield pointing = row_i & ~row_locked;
        if (pointing)
            for (unsigned col = 0; col < 9; ++col)
            {
                if (matrix[i][col] || (col >= col_base && col < col_base + 3))
                    continue;
                memory[i][col] &= ~pointing;
            }

        total_count = 0;
        for (unsigned col = col_base; col < col_base + 3; ++col)
        {
            if (col == j)
            {
                for (unsigned row = row_base; row < row_base + 3; ++row)
                {
                    if (matrix[row][col])
                        continue;
                    col_j |= memory[row][col];
                }
            }
            else
            {
                for (unsigned row = row_base; row < row_base + 3; ++row)
                {
                    if (matrix[row][col])
                        continue;
                    col_locked |= memory[row][col];
                    ++total_count;
                }
            }
        }
        if (total_count && total_count == bitCount(col_locked))
            memory[i][j] &= ~col_locked;
        pointing = col_j & ~col_locked;
        if (pointing)
            for (unsigned row = 0; row < 9; ++row)
            {
                if (matrix[row][j] || (row >= row_base && row < row_base + 3))
                    continue;
                memory[row][j] &= ~pointing;
            }

        //Locked Candidate Type 2 (Claiming)
        for (unsigned col = 0; col < 9; ++col)
        {
            if (matrix[i][col] || (col >= col_base && col < col_base + 3))
                continue;
            row_i &= ~memory[i][col];
        }
        if (row_i)
        {
            for (unsigned r = row_base; r < row_base + 3; ++r)
                for (unsigned c = col_base; c < col_base + 3; ++c)
                {
                    if (r == i || matrix[r][c])
                        continue;
                    memory[r][c] &= ~row_i;
                }
        }

        for (unsigned row = 0; row < 9; ++row)
        {
            if (matrix[row][j] || (row >= row_base && row < row_base + 3))
                continue;
            col_j &= ~memory[row][j];
        }
        if (col_j)
        {
            for (unsigned r = row_base; r < row_base + 3; ++r)
                for (unsigned c = col_base; c < col_base + 3; ++c)
                {
                    if (c == j || matrix[r][c])
                        continue;
                    memory[r][c] &= ~col_j;
                }
        }
    }
    void pair_check(unsigned i, unsigned j)
    {
        bitfield value = memory[i][j];
        if (bitCount(value) == 2)
        {
            unsigned match_r(9), match_c(9);

            //Naked Pair in Row
            for (unsigned col = 0; col < 9; ++col)
            {
                if (matrix[i][col] || col == j || memory[i][col] != value)
                    continue;
                match_c = col;
                break;
            }

            if (match_c != 9)
                for (unsigned col = 0; col < 9; ++col)
                {
                    if (matrix[i][col] || col == j || col == match_c)
                        continue;
                    memory[i][col] &= ~value;
                }

            //Naked Pair in Column
            for (unsigned row = 0; row < 9; ++row)
            {
                if (matrix[row][j] || row == i || memory[row][j] != value)
                    continue;
                match_r = row;
                break;
            }

            if (match_r != 9)
                for (unsigned row = 0; row < 9; ++row)
                {
                    if (matrix[row][j] || row == i || row == match_r)
                        continue;
                    memory[row][j] &= ~value;
                }

            match_r = 9, match_c = 9;
            //Naked Pair in House
            unsigned row_base = i / 3 * 3;
            unsigned col_base = j / 3 * 3;
            for (unsigned row = row_base; row < row_base + 3; ++row)
            {
                if (row == i)
                    continue;
                for (unsigned col = col_base; col < col_base + 3; ++col)
                {
                    if (col == j || matrix[row][col] || memory[row][col] != value)
                        continue;
                    match_r = row, match_c = col;
                    break;
                }
            }

            if (match_r != 9)
                for (unsigned row = row_base; row < row_base + 3; ++row)
                {
                    for (unsigned col = col_base; col < col_base + 3; ++col)
                    {
                        if (matrix[row][col] || (row == i && col == j) ||
                                (row == match_r && col == match_c))
                            continue;
                        memory[row][col] &= ~value;
                    }
                }
        }
    }
    void advanced_fill()
    {
        bool again;
        unsigned count = 0;
        unsigned sum;
        for (unsigned i = 0; i < 9; ++i)
            for (unsigned j = 0; j < 9; ++j)
                if (!matrix[i][j])
                    memory[i][j] &= Blank.possible(i, j);

        do
        {
            again = false;
            ++count;
            output << "Advanced solving attempt " << count
                   << " ..." << std::endl;
            sum   = 0;
            for (unsigned i = 0; i < 9; ++i)
                for (unsigned j = 0; j < 9; ++j)
                    sum += memory[i][j];

            for (unsigned i = 0; i < 9; ++i)
            {
                for (unsigned j = 0; j < 9; ++j)
                {
                    if (matrix[i][j]) continue;
                    candidate_check(i, j);
                    pair_check(i, j);
                    bitfield possible = memory[i][j];
                    bitfield house = house_check(i, j, true);
                    bitfield row = row_check(i, j, true);
                    bitfield col = col_check(i, j, true);
                    bitfield to_check = decide(possible, house, row, col);
                    if (to_check)
                        set(i, j, numFor(to_check), true);
                }
            }

            for (unsigned i = 0; i < 9; ++i)
                for (unsigned j = 0; j < 9; ++j)
                    sum -= memory[i][j];
            if (sum) again = true;
        }
        while (again);
    }
    unsigned remaining()
    {
        return remains;
    }
    bool backtrack()
    {
        backtrack_count = 0;
        bool result = _btrack(remaining());
        output << "And totally " << backtrack_count
               << " backtracking attempt(s)." << std::endl;
        return result;
    }
    bool assert(unsigned i, unsigned j, unsigned val)
    {
        return matrix[i][j] == val;
    }
    void print_board(std::ostream & out)
    {
        for (unsigned i = 0; i < 9; ++i)
		{
            for (unsigned j = 0; j < 9; ++j)
                out << matrix[i][j] << " ";
			out << std::endl;
		}
		out << std::endl;
    }
private:
    unsigned matrix[9][9];
    bitfield memory[9][9];
    unsigned remains;
    BlankList Blank;

    static unsigned numFor(bitfield bit)
    {
        unsigned num = 0;
        while (bit)
        {
            bit >>= 1;
            ++num;
        }
        return num;
    }
    bool findMin(unsigned& row, unsigned& col, bool & unique)
    {
        bool found = false;
        unsigned count = 10;

        for (unsigned i = 0; i < 9; ++i)
            for (unsigned j = 0; j < 9; ++j)
            {
                if (!matrix[i][j] && (bitCount(Blank.possible(i, j)
                                               & memory[i][j])) < count)
                {
                    count = bitCount(Blank.possible(i, j) & memory[i][j]);
                    row = i;
                    col = j;
                    found = true;
                }
            }
        if (count == 1)
            unique = true;
        return found;
    }
    bool _btrack(unsigned depth)
    {
        unsigned row, col;
        bool unique = false;
        if (!findMin(row, col, unique))
            if (!depth)
                return true;
            else
                return false;

        // Iterate through the possible values this cell could have
        unsigned num = 1;
        bitfield mask = bitFor(num);
        ++backtrack_count;

        while (mask != maskMax)
        {
            if (mask_check(row, col, mask))
            {
                set(row, col, num);
                if (_btrack(depth-1))
                    return true;
                unset(row, col);
                if (unique)
                    break;
            }

            // Advance to the next number
            mask *= 2;
            num++;
        }
        return false;
    }
    void _update(unsigned row, unsigned col)
    {
        unsigned row_base = row / 3 * 3;
        unsigned col_base = col / 3 * 3;
        unsigned r, c;

        for (unsigned i = 0; i < 9; ++i)
        {
            if (!matrix[row][i])
                memory[row][i] &= Blank.possible(row, i);
            if (!matrix[i][col])
                memory[i][col] &= Blank.possible(i, col);
            r = row_base + i / 3;
            c = col_base + i % 3;
            if (r != row && c != col && !matrix[r][c])
                memory[r][c] &= Blank.possible(r, c);
        }
    }
    bool _fill(bool is_big, unsigned block, unsigned num)
    {
        if (is_big)
        {
            if (num == 10)
                return true;
            if (block == 9)
                return _fill(is_big, 0, num + 1);
        }
        if (block == 9)
            return true;
        static unsigned look_up[] =
        {
            0,  1,  2,
            9,  10, 11,
            18, 19, 20
        };
        static unsigned base[] =
        {
            0,  3,  6,
            27, 30, 33,
            54, 57, 60
        };
        unsigned place;
        bitfield available = allSet;
        while (available)
        {
            while (true)
            {
                place = std::rand() % 9;
                if (available & bitFor(place + 1))
                    break;
            }
            available &= ~bitFor(place + 1);

            unsigned loc = base[(2 * block) % 9] + look_up[place];
            unsigned row = loc / 9, col = loc % 9;
            if (!matrix[row][col] &&
                    (bitFor(num) & Blank.possible(row, col)))
            {
                set(row, col, num);
                if (_fill(is_big, block + 1, num))
                    return true;
                unset(row, col);
            }
        }
        return false;
    }

    void _random_fill()
    {
        for (unsigned i = 1; i <= 5; ++i)
            _fill(false, 0, i);
        if (!_fill(true, 0, 6))
            std::cout << "Error" << std::endl;
    }
};

class Holes
{
public:
    Holes(Board & board)
        : puzzle(board)
    {}
    void digHoles(difficulty level)
    {
        static unsigned look_up[] =
        {
            0,  1,  2,
            9,  10, 11,
            18, 19, 20
        };
        static unsigned base[] =
        {
            0,  3,  6,
            27, 30, 33,
            54, 57, 60
        };
        static unsigned array[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        for (unsigned i = 0; i < 9; ++i)
        {

            unsigned diff = _random_by_level(level);
            std::random_shuffle(array, array + 9);
            for (unsigned j = 0; j < diff; ++j)
            {
                unsigned loc = base[i] + look_up[array[j]];
                unsigned row = loc / 9, col = loc % 9;
                if (!_valid_dig(row, col, level))
                {
                    ++diff;
                    if (diff == 10)
                        break;
                }
            }
        }
    }
    Board& to_play()
    {
        return puzzle;
    }
private:
    Board puzzle;
    Board ref;

    unsigned _random_by_level(difficulty level)
    {
        unsigned random = 5;
        int rnd = std::rand();

        switch (level)
        {
        case EASY:
            random = rnd % 7;
            if (random < 4 && rnd % 2)
                random += std::rand() % 4;
            random += 3;
            break;
        case MEDIUM:
            random = rnd % 5 + 3;
            break;
        case DIFFICULT:
            random = rnd % 5 + 4;
            break;
        case EVIL:
            random = rnd % 5 + 5;
            break;
        }
        return random;
    }

    bool _valid_dig(unsigned i, unsigned j, difficulty level)
    {
        unsigned val = puzzle.unset(i, j);
		if (level == EASY)
		{
			Board bd = puzzle;
			bd.hidden_fill();
			if (bd.remaining())
			{
				puzzle.set(i, j, val);
				return false;
			}
		}
        for (unsigned num = 1; num <= 9; ++num)
        {
            if (num != val &&
                    puzzle.mask_check(i, j, bitFor(num)))
            {
                Board bd = puzzle;
                bd.set(i, j, num);
                bd.hidden_fill();
				bd.advanced_fill();
                if (bd.backtrack())
                {
                    puzzle.set(i, j, val);
                    return false;
                }
            }
        }

		return true;
    }
};

class Sudoku
{
public:
    Sudoku(const char * name, bool play = false)
    {
        std::ifstream in(name);
        unsigned num = 0;
        for (unsigned i = 0; i < 9; ++i)
        {
            for (unsigned j = 0; j < 9; ++j)
            {
                in >> num;
                _board.set(i, j, num);
            }
        }
        _board.print_board(std::cout);
        _answer = _board;
        if (_solve())
        {
            if (!play)
            {
                std::cout << "The answer is:" << std::endl;
                _answer.print_board(std::cout);
            }
        }
        else
            std::cout << "The sudoku is not solvable!" << std::endl;
    }
    Sudoku(difficulty level, std::ostream & out)
    {
        output << "Generating new puzzle:" << std::endl;
        generate(level, out);
    }
    void generate(difficulty level, std::ostream & out)
    {
        _answer = Board(static_cast<unsigned>(std::time(0)));
        while (true)
        {
            Holes game(_answer);
            game.digHoles(level);
            Board bd = game.to_play();
            if (level > EASY)
            {
                bd.hidden_fill();
                if (!bd.remaining())
                    continue;
                bd.advanced_fill();
                if (level > MEDIUM && !bd.remaining())
                    continue;
            }
            _board = game.to_play();
            break;
        }
        _board.print_board(out);
    }
    void play(unsigned& row, unsigned& col, unsigned& val)
    {
        if (_board.mask_check(row, col, bitFor(val)))
            if (_answer.assert(row, col, val))
            {
                _board.set(row, col, val);
                system("cls");
                _board.print_board(std::cout);
            }
            else
                std::cout << "You've chosen the wrong number.\n";
        else
            std::cout << "Your play violated the rules.\n";
    }
    bool is_complete()
    {
        return _board.remaining() == 0;
    }
private:
    Board _board;
    Board _answer;

    bool _solve()
    {
        output << "Solving puzzle:" << std::endl;
        _answer.hidden_fill();
        if (!_answer.remaining())
            return true;
        _answer.advanced_fill();
        if (_answer.backtrack())
            return true;
        else
            return false;
    }
};

int main()
{
    char mode = 0;
    std::cout << "Input puzzle file (I) or Generate one (G): ";
    std::cin >> mode;
    if (mode == 'I')
    {
        char file[100];
        bool flag;
        std::cout << "Please specify the file: ";
        std::cin >> file;
AM:
        std::cout << "Automatically (A) solve it or Manually (M): ";
        std::cin >> mode;
        if (mode == 'A')
            flag = false;
        else if (mode == 'M')
            flag = true;
        else
        {
            std::cout << "Invalid Input!\n";
            goto AM;
        }

        system("cls");
        Sudoku puzzle(file, flag);
        if (flag)
        {
            unsigned row, col, val;
            while (!puzzle.is_complete())
            {
                std::cout << "Enter a position and a number (i, j, num): ";
                std::cin >> row >> col >> val;
                puzzle.play(row, col, val);
            }
            std::cout << "You have completed the puzzle." << std::endl;
        }
    }
    else if (mode == 'G')
    {
        std::cout << "Which Level: Easy (E) Medium (M) Difficult(D) Evil(U) ";
        std::cin >> mode;
        difficulty level;
        bool flag;
        switch (mode)
        {
        case 'E':
            level = EASY;
            break;
        case 'M':
            level = MEDIUM;
            break;
        case 'D':
            level = DIFFICULT;
            break;
        case 'U':
            level = EVIL;
            break;
        default:
            level = DEFAULT;
            break;
        }
SP:
        std::cout << "Save it to a file (S) or Play now (P): ";
        std::cin >> mode;
        if (mode == 'S')
            flag = false;
        else if (mode == 'P')
            flag = true;
        else
        {
            std::cout << "Invalid Input!\n";
            goto SP;
        }

        if (flag)
        {
            system("cls");
            Sudoku puzzle(level, std::cout);
            unsigned row, col, val;
            while (!puzzle.is_complete())
            {
                std::cout << "Enter a position and a number (i, j, num): ";
                std::cin >> row >> col >> val;
                puzzle.play(row, col, val);
            }
            std::cout << "You have completed the puzzle." << std::endl;
        }
        else
        {
            std::ofstream ofs("Sudoku.out");
            Sudoku puzzle(level, ofs);
            std::cout << "Saved as Sudoku.out" << std::endl;
        }
    }
    system("pause");
}
