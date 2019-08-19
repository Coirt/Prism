#include "Rainbow.hpp"

using namespace rainbow;

float Audio::generateNoise() {
	float nO;
	switch (noiseSelected) {
		case 0:
			nO = brown.next() * 10.0f - 5.0f;
			break;
		case 1:
			nO = pink.next() * 10.0f - 5.0f;
			break;
		case 2:
			nO = white.next() * 10.0f - 5.0f;
			break;
		default:
			nO = pink.next() * 10.0f - 5.0f;
	}
	return nO;
}

void Audio::nChannelProcess(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output) {

    // Must generate 2, 3 or 6 input streams
    switch(inputChannels) {
            case 0:
            case 1:
            case 2:
                    inChannels = 2;
                    break;
            case 3:
                    inChannels = 3;
                    break;
            default:
                    inChannels = 6;
    }

    // Must generate 1, 2 or 6 output streams
    switch(outputChannels) {
            case 0:
                    outChannels = 1;
                    break;
            case 1:
                    outChannels = 2;
                    break;
            case 2:
                    outChannels = 6;
                    break;
            default:
                    outChannels = 1;
    }

	populateInputBuffer(input);

	// At this point we have populated 2,3 or 6 buffers
	// Process buffer
	if (nOutputBuffer[0].empty()) {
		resampleInput(main);
		// Pass to module
		main.process_audio();
		populateAndResampleOutputBuffer(main);
	}

	// Set output
	if (!nOutputBuffer[0].empty()) {
		processOutputBuffer(output);
	}
}


// Populate input buffer
// Do not populate if we should not add more data to channels 2-6
// OK
void Audio::populateInputBuffer(rack::engine::Input &input) {
	for (int i = 0; i < inChannels; i++) {
		if (!nInputBuffer[i].full()) {
			if (inputChannels == 0) {
				nInputFrame[i].samples[0] = generateNoise() / 5.0f;
			} else if (inputChannels == 1) {
				nInputFrame[i].samples[0] = input.getVoltage(0) / 5.0f;
			} else {
				nInputFrame[i].samples[0] = input.getVoltage(i) / 5.0f;
			}
			nInputBuffer[i].push(nInputFrame[i]);
		} 
	}
}

// Process input buffer
void Audio::resampleInput(rainbow::Controller &main) {

	// We need to flush other buffers!
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (!nInputBuffer[i].empty()) {

			nInputSrc[i].setRates(sampleRate, 96000);

			int inLen = nInputBuffer[i].size();
			int outLen = NUM_SAMPLES;
			nInputSrc[i].process(nInputBuffer[i].startData(), &inLen, nInputFrames[i], &outLen);
			nInputBuffer[i].startIncr(inLen);

			for (int j = 0; j < NUM_SAMPLES; j++) {
				int32_t v = (int32_t)clamp(nInputFrames[i][j].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);

				switch(inChannels) {
					case 2:
						main.io->in[i][j] 		= v;
						main.io->in[2 + i][j] 	= v;
						main.io->in[4 + i][j] 	= v;
						break;
					case 3:
						main.io->in[i][j] 		= v;
						main.io->in[1 + i][j] 	= v;
						break;
					default:
						main.io->in[i][j] 		= v;
				}
			}
		} 
	}
}

void Audio::populateAndResampleOutputBuffer(rainbow::Controller &main) {

	// Convert output buffer
	for (int chan = 0; chan < NUM_CHANNELS; chan++) {
		for (int i = 0; i < NUM_SAMPLES; i++) {
			switch(outputChannels) {
				case 0:
					nOutputFrames[0][i].samples[0] += main.io->out[chan][i] / MAX_12BIT;
					break;
				case 1:
					if (i & 1) {
						nOutputFrames[1][i].samples[0] += main.io->out[chan][i] / MAX_12BIT;
					} else {
						nOutputFrames[0][i].samples[0] += main.io->out[chan][i] / MAX_12BIT;
					}
					break;
				case 2:
					nOutputFrames[chan][i].samples[0] = main.io->out[chan][i] / MAX_12BIT; //assign not add
					break;
				default:
					nOutputFrames[0][i].samples[0] += main.io->out[chan][i] / MAX_12BIT;
					break;
			}

			nOutputSrc[chan].setRates(96000, sampleRate);
			int inLen = NUM_SAMPLES;
			int outLen = nOutputBuffer[chan].capacity();
			nOutputSrc[chan].process(nOutputFrames[chan], &inLen, nOutputBuffer[chan].endData(), &outLen);
			nOutputBuffer[chan].endIncr(outLen);

		}
	}
}

void Audio::processOutputBuffer(rack::engine::Output &output) {

	output.setChannels(outChannels);

	bool bufferReady = true;
	for (int chan = 0; chan < outChannels; chan++) {
		bufferReady = bufferReady && !nOutputBuffer[chan].empty();
	}

	// Set output
	if (bufferReady) {

		for (int chan = 0; chan < outChannels; chan++) {
			nOutputFrame[chan] = nOutputBuffer[chan].shift();
		}

		switch(outputChannels) {
			case 0:
				output.setVoltage(nOutputFrame[0].samples[0] * 5.0f, 0);
				break;
			case 1:
				output.setVoltage(nOutputFrame[0].samples[0] * 5.0f, 0);
				output.setVoltage(nOutputFrame[1].samples[0] * 5.0f, 1);
				break;
			case 2:
				for (int chan = 0; chan < outChannels; chan++) {
					output.setVoltage(nOutputFrame[chan].samples[0] * 5.0f, chan);
				}
				break;
			default:
				output.setVoltage(nOutputFrame[0].samples[0] * 5.0f, 0);
				break;
		}
	}
}

