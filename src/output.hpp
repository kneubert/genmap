#include <vector>
#include <string>
#include <cstdint>
#include <tuple>

// TODO: investigate performance of buffer sizes (stack overflow might occur leading to a segmentation fault)
#define     BUFFER_SIZE     32*1024 // 32 KB

using namespace seqan;

//saveConsScore(SearchParams const & searchParams, std::string const & output_path, std::vector<std::tuple<uint64_t, uint64_t, float>> & kmerFrequency);


template <bool mappability, typename T>
void saveRaw(std::vector<T> const & c, std::string const & output_path)
{
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(output_path, std::ios::out | std::ios::binary);
    outfile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    SEQAN_IF_CONSTEXPR (mappability)
    {
        for (T const v : c)
        {
            float const f = (v != 0) ? 1.0f / static_cast<float>(v) : 0;
            outfile.write(reinterpret_cast<const char*>(&f), sizeof(float));
        }
    }
    else
    {
        outfile.write((const char*) &c[0], c.size() * sizeof(T));
    }

    outfile.close();
}

template <bool mappability, typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveTxt(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths)
{
    char buffer[BUFFER_SIZE];
    std::ofstream outfile(output_path + ".txt", std::ios::out | std::ofstream::binary);
    outfile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    auto seqBegin = c.begin();
    auto seqEnd = c.begin() + chromLengths[0];
    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        outfile << '>' << chromNames[i] << '\n';

        SEQAN_IF_CONSTEXPR (mappability)
        {
            for (auto it = seqBegin; it < seqEnd - 1; ++it)
            {
                float const f = (*it != 0) ? 1.0f / static_cast<float>(*it) : 0;
                outfile << f << ' ';
            }
            float const f = (*(seqEnd - 1) != 0) ? 1.0f / static_cast<float>(*(seqEnd - 1)) : 0;
            outfile << f; // no space after last value
        }
        else
        {
            std::copy(seqBegin, seqEnd - 1, std::ostream_iterator<uint16_t>(outfile, " "));
            outfile << static_cast<uint16_t>(*(seqEnd - 1)); // no space after last value
        }
        outfile << '\n';

        if (i + 1 < length(chromLengths))
        {
            seqBegin = seqEnd;
            seqEnd += chromLengths[i + 1];
        }
    }
    outfile.close();
}

template <bool mappability, typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveWig(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths)
{
    uint64_t pos = 0;
    uint64_t begin_pos_string = 0;
    uint64_t end_pos_string = chromLengths[0];

    char buffer[BUFFER_SIZE];

    std::ofstream wigFile(output_path + ".wig");
    wigFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        uint16_t current_val = c[pos];
        uint64_t occ = 0;
        uint64_t last_occ = 0;

        while (pos < end_pos_string + 1) // iterate once more to output the last line
        {
            if (pos == end_pos_string || current_val != c[pos])
            {
                if (last_occ != occ)
                    wigFile << "variableStep chrom=" << chromNames[i] << " span=" << occ << '\n';
                // TODO: document this behavior (mappability of 0)
                SEQAN_IF_CONSTEXPR (mappability)
                {
                    float const value = (current_val != 0) ? 1.0f / static_cast<float>(current_val) : 0;
                    wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << value << '\n'; // pos in wig start at 1
                }
                else
                {
                    wigFile << (pos - occ + 1 - begin_pos_string) << ' ' << current_val << '\n'; // pos in wig start at 1
                }

                last_occ = occ;
                occ = 0;
                if (pos < end_pos_string)
                    current_val = c[pos];
            }

            ++occ;
            ++pos;
        }
        --pos; // pos is incremented once too often by the additional last iteration, i.e., pos == end_pos_string

        begin_pos_string += chromLengths[i];
        if (i + 1 < length(chromLengths))
            end_pos_string += chromLengths[i + 1];
    }
    wigFile.close();

    // .chrom.sizes file
    std::ofstream chromSizesFile(output_path + ".chrom.sizes");
    for (uint64_t i = 0; i < length(chromLengths); ++i)
        chromSizesFile << chromNames[i] << '\t' << chromLengths[i] << '\n';
    chromSizesFile.close();
}

template <bool mappability, typename T, typename TChromosomeNames, typename TChromosomeLengths>
void saveBed(std::vector<T> const & c, std::string const & output_path, TChromosomeNames const & chromNames, TChromosomeLengths const & chromLengths)
{
    uint64_t pos = 0;
    uint64_t begin_pos_string = 0;
    uint64_t end_pos_string = chromLengths[0];

    char buffer[BUFFER_SIZE];

    std::ofstream bedFile(output_path + ".bed");
    bedFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    for (uint64_t i = 0; i < length(chromLengths); ++i)
    {
        uint16_t current_val = c[pos];
        uint64_t occ = 0;

        while (pos < end_pos_string + 1) // iterate once more to output the last line
        {
            if (pos == end_pos_string || current_val != c[pos])
            {
                bedFile << chromNames[i] << '\t'                    // chrom name
                        << (pos - occ - begin_pos_string) << '\t'   // start pos (begins with 0)
                        << (pos - begin_pos_string - 1) << '\t'     // end pos
                        << '-' << '\t';                             // name

                SEQAN_IF_CONSTEXPR (mappability)
                    bedFile << ((current_val != 0) ? 1.0f / static_cast<float>(current_val) : 0) << '\n';
                else
                    bedFile << current_val << '\n';

                occ = 0;
                if (pos < end_pos_string)
                    current_val = c[pos];
            }

            ++occ;
            ++pos;
        }
        --pos; // pos is incremented once too often by the additional last iteration, i.e., pos == end_pos_string

        begin_pos_string += chromLengths[i];
        if (i + 1 < length(chromLengths))
            end_pos_string += chromLengths[i + 1];
    }
    bedFile.close();
}

template <bool mappability, typename TLocations, typename TDirectoryInformation>
void saveCsv(std::string const & output_path, TLocations const & locations,
             SearchParams const & searchParams, TDirectoryInformation const & directoryInformation)
{
    char buffer[BUFFER_SIZE];

    std::ofstream csvFile(output_path + ".csv");
    csvFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    uint64_t chromosomeCount = 0;
    std::vector<std::pair<std::string, uint64_t> > fastaFiles; // fasta file, cumulative nbr. of chromosomes
    std::string lastFastaFile = std::get<0>(retrieveDirectoryInformationLine(directoryInformation[0]));
    for (auto const & row : directoryInformation)
    {
        auto const line = retrieveDirectoryInformationLine(row);
        if (lastFastaFile != std::get<0>(line))
        {
            fastaFiles.push_back({lastFastaFile, chromosomeCount - 1});
            lastFastaFile = std::get<0>(line);
        }
        ++chromosomeCount;
    }

    csvFile << "\"k-mer\"";
    for (auto const & fastaFile : fastaFiles)
        csvFile << ";\"+ strand " << fastaFile.first << "\"";
    if (searchParams.revCompl) // TODO: make it constexpr?
    {
        for (auto const & fastaFile : fastaFiles)
            csvFile << ";\"- strand " << fastaFile.first << "\"";
    }
    csvFile << '\n';

    for (auto const & kmerLocations : locations)
    {
        auto const & kmerPos = kmerLocations.first;
        auto const & plusStrandLoc = kmerLocations.second.first;
        auto const & minusStrandLoc = kmerLocations.second.second;

        csvFile << kmerPos.i1 << ',' << kmerPos.i2;

        uint64_t i = 0;
        uint64_t nbrChromosomesInPreviousFastas = 0;
        for (auto const & fastaFile : fastaFiles)
        {
            csvFile << ';';
            bool subsequentIterations = false;
            while (i < plusStrandLoc.size() && plusStrandLoc[i].i1 <= fastaFile.second)
            {
                if (subsequentIterations)
                    csvFile << '|'; // separator for multiple locations in one column
                csvFile << (plusStrandLoc[i].i1 - nbrChromosomesInPreviousFastas) << ',' << plusStrandLoc[i].i2;
                subsequentIterations = true;
                ++i;
            }
            nbrChromosomesInPreviousFastas = fastaFile.second + 1;
        }

        if (searchParams.revCompl)
        {
            uint64_t i = 0;
            uint64_t nbrChromosomesInPreviousFastas = 0;
            for (auto const & fastaFile : fastaFiles)
            {
                csvFile << ';';
                bool subsequentIterations = false;
                while (i < minusStrandLoc.size() && minusStrandLoc[i].i1 <= fastaFile.second)
                {
                    if (subsequentIterations)
                        csvFile << '|'; // separator for multiple locations in one column
                    csvFile << (minusStrandLoc[i].i1 - nbrChromosomesInPreviousFastas) << ',' << minusStrandLoc[i].i2;
                    subsequentIterations = true;
                    ++i;
                }
                nbrChromosomesInPreviousFastas = fastaFile.second + 1;
            }
        }
        csvFile << '\n';
    }

    csvFile.close();
}

template <bool mappability, typename TLocations, typename TDirectoryInformation>
void saveKmerStatistics(std::string const & output_path, TLocations const & locations,
             SearchParams const & searchParams, TDirectoryInformation const & directoryInformation)
{
    char buffer[BUFFER_SIZE];

    std::ofstream csvFile(output_path + ".stats.csv");
    csvFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    uint64_t chromosomeCount = 0;
    std::vector<std::pair<std::string, uint64_t> > fastaFiles; // fasta file, cumulative nbr. of chromosomes
    std::string lastFastaFile = std::get<0>(retrieveDirectoryInformationLine(directoryInformation[0]));
    for (auto const & row : directoryInformation)
    {
        auto const line = retrieveDirectoryInformationLine(row);
        if (lastFastaFile != std::get<0>(line))
        {
            fastaFiles.push_back({lastFastaFile, chromosomeCount - 1});
            lastFastaFile = std::get<0>(line);
        }
        ++chromosomeCount;
    }

    csvFile << "\"k-mer_chrom\"";
    csvFile << "\t";
    csvFile << "\"k-mer_pos\"";
    csvFile << "\t";
    csvFile << "\"F\"";
    for (auto const & fastaFile : fastaFiles)
        csvFile << "\t\"" << fastaFile.first << "\"";
        //csvFile << "\t\"+ strand " << fastaFile.first << "\"";
    //if (searchParams.revCompl) // TODO: make it constexpr?
    //{
    //    for (auto const & fastaFile : fastaFiles)
    //        csvFile << "\t\"- strand " << fastaFile.first << "\"";
    //}
    csvFile << '\n';

    uint64_t num_genomes = fastaFiles.size();
    std::cout << "processing statistics for " << num_genomes << " genomes" << std::endl;
    std::vector<std::tuple<uint64_t, uint64_t, float> > kmerFrequency;


    for (auto const & kmerLocations : locations)
    {
        auto const & kmerPos = kmerLocations.first;
        auto const & plusStrandLoc = kmerLocations.second.first;
        auto const & minusStrandLoc = kmerLocations.second.second;

        //csvFile << kmerPos.i1 << '\t' << kmerPos.i2;

        //uint64_t i = 0;
        std::vector<uint64_t> plusStrandLocations;
        std::vector<uint64_t> minusStrandLocations;
        std::vector<uint64_t> allStrandLocations;

        /*
        bool non_repeated = true;

        for (auto const & fastaFile : fastaFiles)
        {
            csvFile << '\t';
            uint64_t plusStrandOcc = 0;

            while (i < plusStrandLoc.size() && plusStrandLoc[i].i1 <= fastaFile.second)
            {
                ++i;
                ++plusStrandOcc;
            }
            csvFile << plusStrandOcc;
            plusStrandLocations.push_back(plusStrandOcc);
            if (plusStrandOcc > 1) {
                non_repeated = false;
            }
        }

        if (searchParams.revCompl)
        {
            uint64_t i = 0;
            for (auto const & fastaFile : fastaFiles)
            {
                csvFile << '\t';
                uint64_t minusStrandOcc = 0;

                while (i < minusStrandLoc.size() && minusStrandLoc[i].i1 <= fastaFile.second)
                {
                    ++i;
                    ++minusStrandOcc;
                }
                minusStrandLocations.push_back(minusStrandOcc);
                if (minusStrandOcc > 1) {
                    non_repeated = false;
                }
                csvFile << minusStrandOcc;
            }
        }
        */

        uint64_t i = 0;
        uint64_t j = 0;
        uint64_t f = 0;
        for (auto const & fastaFile : fastaFiles)
        {
            uint64_t allStrandOcc = 0;

            while (i < plusStrandLoc.size() && plusStrandLoc[i].i1 <= fastaFile.second) {
                ++i;
                ++allStrandOcc;
            }

            if (searchParams.revCompl)
            {
                while (j < minusStrandLoc.size() && minusStrandLoc[j].i1 <= fastaFile.second) {
                    ++j;
                    ++allStrandOcc;
                }
            }
            if (allStrandOcc > 0)
            {
                f += 1;
            }
            allStrandLocations.push_back(allStrandOcc);
        }


        //if (non_repeated) {
        csvFile << kmerPos.i1 << '\t' << kmerPos.i2;
        csvFile << '\t' << static_cast<float>(f)/ static_cast<float>(num_genomes);

        float freq = static_cast<float>(f)/ static_cast<float>(num_genomes);
        std::tuple<uint64_t, uint64_t, float> tfreq = std::make_tuple(kmerPos.i1, kmerPos.i2, freq);
        kmerFrequency.push_back(tfreq);

        for (auto const & occ : allStrandLocations)
        {
            csvFile << '\t';
            csvFile << occ;
        }

        csvFile << '\n';

    }

    csvFile.close();

    std::ofstream csvFile2(output_path + ".cp.csv");
    csvFile2.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    csvFile2 << "chr";
    csvFile2 << "\t";
    csvFile2 << "pos";
    csvFile2 << "\t";
    csvFile2 << "Fp";
    csvFile2 << "\t";
    csvFile2 << "Cp";
    csvFile2 << "\n";


    // regions
    std::ofstream csRegionsFile(output_path + ".cs.bed");
    csRegionsFile.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);


    float val = 0;
    uint64_t region_start = 0;
    uint64_t region_end = 0;

    for (auto it = kmerFrequency.begin() + searchParams.length - 1; it != kmerFrequency.end(); ++it)
    {
        auto const &kmerChr = std::get<0>(*(it));
        auto const &kmerPos = std::get<1>(*(it));
        auto const &kmerFreq = std::get<2>(*(it));

        csvFile2 << kmerChr;
        csvFile2 << "\t";
        csvFile2 << kmerPos;
        csvFile2 << "\t";
        csvFile2 << kmerFreq;
        csvFile2 << "\t";

        float max = 0;
        for (auto it2 = it - searchParams.length + 1; it2 < it; ++it2) {
            auto const &kmerFreq2 = std::get<2>(*(it2));
            if (kmerFreq2 > max)
            {
                max = kmerFreq2;
            }
        }
        csvFile2 << max << "\n";
        if (max != val)
        {
            region_end = kmerPos - 1;
            if (val >= 0.95) {
                csRegionsFile << kmerChr;
                csRegionsFile << "\t";
                csRegionsFile << region_start;
                csRegionsFile << "\t";
                csRegionsFile << region_end;
                csRegionsFile << "\n";
            }
            region_start = region_end + 1;
            val = max;
        }

    }
    csvFile2.close();
    csvFile.close();
}
